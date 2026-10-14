#include "encuadre_manager.h"

#include <ui/encuadre/encuadre_publish_dialog.h>
#include <utils/logging.h>

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>


namespace ManagementLayer {

namespace {

const QString kApiBaseUrl = QStringLiteral("https://prod.encuadre.muxp.art/api");
const QString kSessionTokenKey = QStringLiteral("encuadre/session-token");
const QString kLinksGroup = QStringLiteral("encuadre/links");

/**
 * @brief Ключ настроек для привязки локального проекта к проекту и сценарию портала
 */
QString linkKey(const QString& _projectFilePath, const QString& _name)
{
    const auto hash
        = QCryptographicHash::hash(QFileInfo(_projectFilePath).absoluteFilePath().toUtf8(),
                                   QCryptographicHash::Sha1)
              .toHex();
    return QString("%1/%2/%3").arg(kLinksGroup, QString::fromLatin1(hash), _name);
}

/**
 * @brief Безопасное имя файла на основе названия
 */
QString safeFileName(const QString& _name)
{
    QString result = _name;
    result.replace(QRegularExpression(R"([\\/:*?"<>|])"), "_");
    result = result.trimmed();
    return result.isEmpty() ? QStringLiteral("guion") : result.left(120);
}

/**
 * @brief Ответ API
 */
struct ApiResponse {
    int status = 0;
    QJsonValue data;
    QString errorCode;
    QString errorMessage;

    bool isOk() const
    {
        return status >= 200 && status < 300;
    }
};

} // namespace


class EncuadreManager::Implementation
{
public:
    Implementation(EncuadreManager* _q, QWidget* _parentWidget);

    /**
     * @brief Выполнить запрос к API
     */
    void call(const QByteArray& _method, const QString& _path, const QJsonObject& _body,
              std::function<void(const ApiResponse&)> _callback,
              const QList<QPair<QByteArray, QByteArray>>& _headers = {});

    /**
     * @brief Текст ошибки для пользователя
     */
    QString errorText(const ApiResponse& _response) const;

    //
    // Шаги работы с порталом
    //
    void checkSession();
    void startConnection();
    void pollConnection();
    void loadProjects();
    void loadScripts(const QString& _projectId);
    void startPublishing();
    void createScriptIfNeeded(std::function<void(const QString&)> _callback);
    void uploadFile(const QJsonObject& _target, const QString& _filePath,
                    std::function<void(bool)> _callback);
    void logout();
    void closeDialog();

    QString sessionToken() const;
    void setSessionToken(const QString& _token);


    EncuadreManager* q = nullptr;
    QWidget* topLevelWidget = nullptr;
    QNetworkAccessManager network;

    QPointer<Ui::EncuadrePublishDialog> dialog;
    PublishRequest request;

    /**
     * @brief Состояние подключения через браузер
     */
    QString connectionRequestId;
    QString connectionExchangeSecret;
    QTimer connectionPollTimer;
    QDateTime connectionExpiresAt;

    /**
     * @brief Временные файлы публикуемой версии
     */
    QScopedPointer<QTemporaryDir> temporaryDir;
    QString pdfFilePath;
    QString sourceFilePath;
};

EncuadreManager::Implementation::Implementation(EncuadreManager* _q, QWidget* _parentWidget)
    : q(_q)
    , topLevelWidget(_parentWidget)
{
    connectionPollTimer.setInterval(2000);
    connectionPollTimer.setSingleShot(true);
    QObject::connect(&connectionPollTimer, &QTimer::timeout, q, [this] { pollConnection(); });
}

void EncuadreManager::Implementation::call(const QByteArray& _method, const QString& _path,
                                           const QJsonObject& _body,
                                           std::function<void(const ApiResponse&)> _callback,
                                           const QList<QPair<QByteArray, QByteArray>>& _headers)
{
    QNetworkRequest networkRequest(QUrl(kApiBaseUrl + _path));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setTransferTimeout(30000);
    const auto token = sessionToken();
    if (!token.isEmpty()) {
        networkRequest.setRawHeader("X-Encuadre-Session", token.toUtf8());
    }
    for (const auto& header : _headers) {
        networkRequest.setRawHeader(header.first, header.second);
    }

    const auto body = _body.isEmpty() && _method == "GET"
        ? QByteArray()
        : QJsonDocument(_body).toJson(QJsonDocument::Compact);
    auto reply = network.sendCustomRequest(networkRequest, _method, body);
    QObject::connect(reply, &QNetworkReply::finished, q, [reply, _callback] {
        reply->deleteLater();
        ApiResponse response;
        response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto json = QJsonDocument::fromJson(reply->readAll()).object();
        response.data = json.value("data");
        response.errorCode = json.value("error").toString();
        response.errorMessage = json.value("message").toString();
        if (response.status == 0) {
            response.errorCode = QStringLiteral("NETWORK_ERROR");
            response.errorMessage = reply->errorString();
        }
        _callback(response);
    });
}

QString EncuadreManager::Implementation::errorText(const ApiResponse& _response) const
{
    if (_response.errorCode == "NETWORK_ERROR") {
        return EncuadreManager::tr("No fue posible conectar con el portal de Encuadre: %1")
            .arg(_response.errorMessage);
    }
    if (_response.errorCode == "CONNECTION_REQUIRED") {
        return EncuadreManager::tr("La sesión con el portal venció. Conéctate de nuevo.");
    }
    if (_response.errorCode == "FORBIDDEN") {
        return EncuadreManager::tr("Tu cuenta no tiene permiso para esta acción en el portal.");
    }
    return EncuadreManager::tr("El portal respondió con un error (%1): %2")
        .arg(_response.errorCode.isEmpty() ? QString::number(_response.status)
                                           : _response.errorCode,
             _response.errorMessage);
}

QString EncuadreManager::Implementation::sessionToken() const
{
    return QSettings().value(kSessionTokenKey).toString();
}

void EncuadreManager::Implementation::setSessionToken(const QString& _token)
{
    QSettings settings;
    if (_token.isEmpty()) {
        settings.remove(kSessionTokenKey);
    } else {
        settings.setValue(kSessionTokenKey, _token);
    }
}

void EncuadreManager::Implementation::checkSession()
{
    if (sessionToken().isEmpty()) {
        dialog->showDisconnected(EncuadreManager::tr(
            "Conecta Encuadre Script con tu cuenta del portal de producción para publicar "
            "guiones en los proyectos del grupo."));
        return;
    }

    dialog->showBusy(EncuadreManager::tr("Verificando la conexión con el portal..."));
    call("GET", "/auth/session", {}, [this](const ApiResponse& _response) {
        if (dialog.isNull()) {
            return;
        }
        if (_response.status == 401) {
            setSessionToken({});
            checkSession();
            return;
        }
        if (!_response.isOk()) {
            dialog->showDisconnected(errorText(_response));
            return;
        }

        dialog->showReady(_response.data.toObject().value("email").toString());
        loadProjects();
    });
}

void EncuadreManager::Implementation::startConnection()
{
    dialog->showBusy(EncuadreManager::tr("Preparando la conexión..."));
    const QJsonObject body{ { "clientName", "Encuadre Script" }, { "clientType", "desktop" } };
    call("POST", "/auth/connect", body, [this](const ApiResponse& _response) {
        if (dialog.isNull()) {
            return;
        }
        if (!_response.isOk()) {
            dialog->showDisconnected(errorText(_response));
            return;
        }

        const auto data = _response.data.toObject();
        connectionRequestId = data.value("requestId").toString();
        connectionExchangeSecret = data.value("exchangeSecret").toString();
        connectionExpiresAt
            = QDateTime::fromString(data.value("expiresAt").toString(), Qt::ISODateWithMs);
        QDesktopServices::openUrl(QUrl(data.value("authUrl").toString()));
        dialog->showWaitingForApproval();
        connectionPollTimer.start();
    });
}

void EncuadreManager::Implementation::pollConnection()
{
    if (dialog.isNull() || connectionRequestId.isEmpty()) {
        return;
    }
    if (connectionExpiresAt.isValid() && QDateTime::currentDateTimeUtc() > connectionExpiresAt) {
        connectionRequestId.clear();
        connectionExchangeSecret.clear();
        dialog->showDisconnected(
            EncuadreManager::tr("La solicitud de conexión venció. Inténtalo de nuevo."));
        return;
    }

    //
    // Секрет обмена передаётся только в заголовке и не сохраняется
    //
    setSessionToken({});
    call("POST", QString("/auth/connect/%1/exchange").arg(connectionRequestId), {},
         [this](const ApiResponse& _response) {
             if (dialog.isNull()) {
                 return;
             }
             if (_response.errorCode == "CONNECTION_PENDING") {
                 connectionPollTimer.start();
                 return;
             }

             connectionRequestId.clear();
             connectionExchangeSecret.clear();
             if (!_response.isOk()) {
                 dialog->showDisconnected(errorText(_response));
                 return;
             }

             setSessionToken(_response.data.toObject().value("sessionToken").toString());
             Log::info("[Encuadre] Desktop session connected");
             checkSession();
         },
         { { "X-Connection-Secret", connectionExchangeSecret.toUtf8() } });
}

void EncuadreManager::Implementation::loadProjects()
{
    call("GET", "/projects", {}, [this](const ApiResponse& _response) {
        if (dialog.isNull()) {
            return;
        }
        if (!_response.isOk()) {
            dialog->showError(errorText(_response));
            return;
        }

        QVector<QPair<QString, QString>> projects;
        for (const auto& value : _response.data.toArray()) {
            const auto project = value.toObject();
            projects.append(
                { project.value("id").toString(), project.value("name").toString() });
        }
        if (projects.isEmpty()) {
            dialog->showError(
                EncuadreManager::tr("No hay proyectos activos en el portal de Encuadre."));
            return;
        }

        const auto savedProjectId
            = QSettings().value(linkKey(request.projectFilePath, "project")).toString();
        dialog->setProjects(projects, savedProjectId);
        loadScripts(dialog->projectId());
    });
}

void EncuadreManager::Implementation::loadScripts(const QString& _projectId)
{
    if (_projectId.isEmpty()) {
        return;
    }

    call("GET", QString("/projects/%1/scripts").arg(_projectId), {},
         [this, _projectId](const ApiResponse& _response) {
             if (dialog.isNull() || dialog->projectId() != _projectId) {
                 return;
             }
             if (!_response.isOk()) {
                 dialog->showError(errorText(_response));
                 return;
             }

             QVector<QPair<QString, QString>> scripts;
             for (const auto& value : _response.data.toArray()) {
                 const auto script = value.toObject();
                 scripts.append(
                     { script.value("id").toString(), script.value("name").toString() });
             }
             QSettings settings;
             const auto savedScriptId
                 = settings.value(linkKey(request.projectFilePath, "project")).toString()
                     == _projectId
                 ? settings.value(linkKey(request.projectFilePath, "script")).toString()
                 : QString();
             dialog->setScripts(scripts, savedScriptId, request.projectName);
         });
}

void EncuadreManager::Implementation::createScriptIfNeeded(
    std::function<void(const QString&)> _callback)
{
    if (!dialog->scriptId().isEmpty()) {
        _callback(dialog->scriptId());
        return;
    }

    const QJsonObject body{ { "name", dialog->newScriptName() } };
    call("POST", QString("/projects/%1/scripts").arg(dialog->projectId()), body,
         [this, _callback](const ApiResponse& _response) {
             if (dialog.isNull()) {
                 return;
             }
             if (!_response.isOk()) {
                 dialog->showError(errorText(_response));
                 return;
             }
             _callback(_response.data.toObject().value("id").toString());
         });
}

void EncuadreManager::Implementation::uploadFile(const QJsonObject& _target,
                                                 const QString& _filePath,
                                                 std::function<void(bool)> _callback)
{
    QFile file(_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        _callback(false);
        return;
    }
    const auto content = file.readAll();
    file.close();

    QNetworkRequest uploadRequest(QUrl(_target.value("uploadUrl").toString()));
    uploadRequest.setTransferTimeout(10 * 60 * 1000);
    const auto headers = _target.value("headers").toObject();
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        uploadRequest.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
    }
    auto reply = network.put(uploadRequest, content);
    QObject::connect(reply, &QNetworkReply::finished, q, [reply, _callback] {
        reply->deleteLater();
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status < 200 || status >= 300) {
            Log::warning("[Encuadre] Upload failed with status %1: %2", QString::number(status),
                         QString::fromUtf8(reply->readAll().left(500)));
        }
        _callback(status >= 200 && status < 300);
    });
}

void EncuadreManager::Implementation::startPublishing()
{
    if (dialog->projectId().isEmpty()) {
        return;
    }

    //
    // Готовим файлы: PDF выбранного драфта и копию файла проекта
    //
    dialog->showBusy(EncuadreManager::tr("Exportando el PDF del guion..."));
    QApplication::processEvents();

    temporaryDir.reset(new QTemporaryDir);
    const auto baseName = safeFileName(request.projectName);
    pdfFilePath = QDir(temporaryDir->path()).filePath(baseName + ".pdf");
    sourceFilePath = QDir(temporaryDir->path()).filePath(baseName + ".encdre");
    if (!temporaryDir->isValid() || !request.exportPdf
        || !request.exportPdf(dialog->draftIndex(), pdfFilePath) || !QFile::exists(pdfFilePath)) {
        dialog->showError(EncuadreManager::tr("No fue posible exportar el PDF del guion."));
        return;
    }
    if (!QFile::copy(request.projectFilePath, sourceFilePath)) {
        dialog->showError(EncuadreManager::tr("No fue posible preparar el archivo .encdre."));
        return;
    }

    dialog->showBusy(EncuadreManager::tr("Preparando la versión en el portal..."));
    createScriptIfNeeded([this](const QString& _scriptId) {
        const auto projectId = dialog->projectId();
        QJsonObject body{
            { "tag", dialog->tag() },
            { "notes", dialog->notes() },
            { "pdf",
              QJsonObject{ { "fileName", QFileInfo(pdfFilePath).fileName() },
                           { "size", QFileInfo(pdfFilePath).size() } } },
            { "source",
              QJsonObject{ { "fileName", QFileInfo(sourceFilePath).fileName() },
                           { "size", QFileInfo(sourceFilePath).size() } } },
            { "appVersion", QApplication::applicationVersion() },
        };
        if (!dialog->versionNumber().isEmpty()) {
            body.insert("versionNumber", dialog->versionNumber());
        }

        const auto versionsPath
            = QString("/projects/%1/scripts/%2/versions").arg(projectId, _scriptId);
        call("POST", versionsPath, body, [this, projectId, _scriptId,
                                          versionsPath](const ApiResponse& _response) {
            if (dialog.isNull()) {
                return;
            }
            if (!_response.isOk()) {
                dialog->showError(errorText(_response));
                return;
            }

            const auto upload = _response.data.toObject();
            const auto uploadId = upload.value("uploadId").toString();
            dialog->showBusy(EncuadreManager::tr("Subiendo el PDF..."));
            uploadFile(upload.value("pdf").toObject(), pdfFilePath, [=](bool _pdfUploaded) {
                if (dialog.isNull()) {
                    return;
                }
                if (!_pdfUploaded) {
                    dialog->showError(EncuadreManager::tr("No fue posible subir el PDF."));
                    return;
                }

                dialog->showBusy(EncuadreManager::tr("Subiendo el archivo .encdre..."));
                uploadFile(
                    upload.value("source").toObject(), sourceFilePath,
                    [=](bool _sourceUploaded) {
                        if (dialog.isNull()) {
                            return;
                        }
                        if (!_sourceUploaded) {
                            dialog->showError(
                                EncuadreManager::tr("No fue posible subir el archivo .encdre."));
                            return;
                        }

                        dialog->showBusy(EncuadreManager::tr("Registrando la versión..."));
                        call("POST", QString("%1/%2/commit").arg(versionsPath, uploadId), {},
                             [=](const ApiResponse& _commitResponse) {
                                 if (dialog.isNull()) {
                                     return;
                                 }
                                 if (!_commitResponse.isOk()) {
                                     dialog->showError(errorText(_commitResponse));
                                     return;
                                 }

                                 QSettings settings;
                                 settings.setValue(linkKey(request.projectFilePath, "project"),
                                                   projectId);
                                 settings.setValue(linkKey(request.projectFilePath, "script"),
                                                   _scriptId);
                                 temporaryDir.reset();

                                 const auto versionNumber = _commitResponse.data.toObject()
                                                                .value("versionNumber")
                                                                .toString();
                                 Log::info("[Encuadre] Script version %1 published",
                                           versionNumber);
                                 dialog->showPublished(
                                     EncuadreManager::tr(
                                         "Listo: la versión %1 del guion ya está en el portal "
                                         "de Encuadre, con su PDF y el archivo .encdre.")
                                         .arg(versionNumber));
                             });
                    });
            });
        });
    });
}

void EncuadreManager::Implementation::logout()
{
    if (!sessionToken().isEmpty()) {
        call("DELETE", "/auth/session", {}, [](const ApiResponse&) {});
    }
    setSessionToken({});
    if (!dialog.isNull()) {
        dialog->showDisconnected(
            EncuadreManager::tr("Sesión cerrada. Conéctate de nuevo para publicar guiones."));
    }
}

void EncuadreManager::Implementation::closeDialog()
{
    connectionPollTimer.stop();
    connectionRequestId.clear();
    connectionExchangeSecret.clear();
    temporaryDir.reset();
    if (!dialog.isNull()) {
        dialog->hideDialog();
    }
}


// ****


EncuadreManager::EncuadreManager(QObject* _parent, QWidget* _parentWidget)
    : QObject(_parent)
    , d(new Implementation(this, _parentWidget))
{
}

EncuadreManager::~EncuadreManager() = default;

void EncuadreManager::publish(const PublishRequest& _request)
{
    if (!d->dialog.isNull()) {
        return;
    }

    d->request = _request;
    d->dialog = new Ui::EncuadrePublishDialog(d->topLevelWidget);
    d->dialog->setDrafts(_request.draftNames);
    connect(d->dialog, &Ui::EncuadrePublishDialog::connectPressed, this,
            [this] { d->startConnection(); });
    connect(d->dialog, &Ui::EncuadrePublishDialog::logoutPressed, this, [this] { d->logout(); });
    connect(d->dialog, &Ui::EncuadrePublishDialog::projectSelected, this,
            [this](const QString& _projectId) { d->loadScripts(_projectId); });
    connect(d->dialog, &Ui::EncuadrePublishDialog::publishPressed, this,
            [this] { d->startPublishing(); });
    connect(d->dialog, &Ui::EncuadrePublishDialog::cancelPressed, this,
            [this] { d->closeDialog(); });
    connect(d->dialog, &Ui::EncuadrePublishDialog::disappeared, d->dialog,
            &Ui::EncuadrePublishDialog::deleteLater);

    d->dialog->showDialog();
    d->checkSession();
}

} // namespace ManagementLayer
