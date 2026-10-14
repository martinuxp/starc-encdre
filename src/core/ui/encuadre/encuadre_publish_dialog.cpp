#include "encuadre_publish_dialog.h"

#include <ui/design_system/design_system.h>
#include <ui/widgets/button/button.h>
#include <ui/widgets/combo_box/combo_box.h>
#include <ui/widgets/label/label.h>
#include <ui/widgets/text_field/text_field.h>
#include <utils/helpers/ui_helper.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QStandardItemModel>


namespace Ui {

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
const QString kNewScriptId = QStringLiteral("__new__");

enum class State {
    Disconnected,
    WaitingForApproval,
    Ready,
    Busy,
    Published,
};
} // namespace

class EncuadrePublishDialog::Implementation
{
public:
    explicit Implementation(QWidget* _parent);

    /**
     * @brief Обновить видимость и доступность элементов под текущее состояние
     */
    void updateState(State _state);

    State state = State::Disconnected;

    Body1Label* status = nullptr;
    ComboBox* project = nullptr;
    QStandardItemModel* projectModel = nullptr;
    ComboBox* script = nullptr;
    QStandardItemModel* scriptModel = nullptr;
    TextField* newScriptName = nullptr;
    ComboBox* draft = nullptr;
    QStandardItemModel* draftModel = nullptr;
    ComboBox* tag = nullptr;
    QStandardItemModel* tagModel = nullptr;
    TextField* versionNumber = nullptr;
    TextField* notes = nullptr;

    QHBoxLayout* buttonsLayout = nullptr;
    Button* logoutButton = nullptr;
    Button* cancelButton = nullptr;
    Button* connectButton = nullptr;
    Button* publishButton = nullptr;
};

EncuadrePublishDialog::Implementation::Implementation(QWidget* _parent)
    : status(new Body1Label(_parent))
    , project(new ComboBox(_parent))
    , projectModel(new QStandardItemModel(project))
    , script(new ComboBox(_parent))
    , scriptModel(new QStandardItemModel(script))
    , newScriptName(new TextField(_parent))
    , draft(new ComboBox(_parent))
    , draftModel(new QStandardItemModel(draft))
    , tag(new ComboBox(_parent))
    , tagModel(new QStandardItemModel(tag))
    , versionNumber(new TextField(_parent))
    , notes(new TextField(_parent))
    , buttonsLayout(new QHBoxLayout)
    , logoutButton(new Button(_parent))
    , cancelButton(new Button(_parent))
    , connectButton(new Button(_parent))
    , publishButton(new Button(_parent))
{
    project->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    project->setModel(projectModel);
    script->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    script->setModel(scriptModel);
    newScriptName->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    draft->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    draft->setModel(draftModel);
    tag->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    tag->setModel(tagModel);
    versionNumber->setSpellCheckPolicy(SpellCheckPolicy::Manual);
    notes->setEnterMakesNewLine(true);

    for (const auto& tagId : { "draft", "review", "approved", "final" }) {
        auto item = new QStandardItem;
        item->setData(QString::fromLatin1(tagId), kIdRole);
        tagModel->appendRow(item);
    }
    tag->setCurrentIndex(tagModel->index(0, 0));

    buttonsLayout->setContentsMargins({});
    buttonsLayout->setSpacing(0);
    buttonsLayout->addWidget(logoutButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(cancelButton);
    buttonsLayout->addWidget(connectButton);
    buttonsLayout->addWidget(publishButton);
}

void EncuadrePublishDialog::Implementation::updateState(State _state)
{
    state = _state;

    const bool isReady = state == State::Ready;
    const bool showForm = state == State::Ready || state == State::Busy;
    for (auto widget : std::vector<QWidget*>{ project, script, tag, versionNumber, notes }) {
        widget->setVisible(showForm);
        widget->setEnabled(isReady);
    }
    const bool isNewScript = script->currentIndex().data(kIdRole).toString() == kNewScriptId;
    newScriptName->setVisible(showForm && isNewScript);
    newScriptName->setEnabled(isReady);
    draft->setVisible(showForm && draftModel->rowCount() > 1);
    draft->setEnabled(isReady);

    logoutButton->setVisible(showForm || state == State::Published);
    logoutButton->setEnabled(isReady || state == State::Published);
    connectButton->setVisible(state == State::Disconnected);
    publishButton->setVisible(showForm);
    publishButton->setEnabled(isReady && projectModel->rowCount() > 0);
    cancelButton->setEnabled(state != State::Busy);
}


// ****


EncuadrePublishDialog::EncuadrePublishDialog(QWidget* _parent)
    : AbstractDialog(_parent)
    , d(new Implementation(this))
{
    setRejectButton(d->cancelButton);

    contentsLayout()->setContentsMargins({});
    contentsLayout()->setSpacing(0);
    int row = 0;
    contentsLayout()->addWidget(d->status, row++, 0);
    contentsLayout()->addWidget(d->project, row++, 0);
    contentsLayout()->addWidget(d->script, row++, 0);
    contentsLayout()->addWidget(d->newScriptName, row++, 0);
    contentsLayout()->addWidget(d->draft, row++, 0);
    contentsLayout()->addWidget(d->tag, row++, 0);
    contentsLayout()->addWidget(d->versionNumber, row++, 0);
    contentsLayout()->addWidget(d->notes, row++, 0);
    contentsLayout()->setRowStretch(row++, 1);
    contentsLayout()->addLayout(d->buttonsLayout, row++, 0);

    connect(d->project, &ComboBox::currentIndexChanged, this, [this](const QModelIndex& _index) {
        if (d->state == State::Ready) {
            emit projectSelected(_index.data(kIdRole).toString());
        }
    });
    connect(d->script, &ComboBox::currentIndexChanged, this,
            [this] { d->updateState(d->state); });
    connect(d->newScriptName, &TextField::textChanged, this,
            [this] { d->newScriptName->clearError(); });
    connect(d->connectButton, &Button::clicked, this, &EncuadrePublishDialog::connectPressed);
    connect(d->logoutButton, &Button::clicked, this, &EncuadrePublishDialog::logoutPressed);
    connect(d->cancelButton, &Button::clicked, this, &EncuadrePublishDialog::cancelPressed);
    connect(d->publishButton, &Button::clicked, this, [this] {
        if (scriptId().isEmpty() && newScriptName().isEmpty()) {
            d->newScriptName->setError(tr("Escribe el nombre del nuevo guion."));
            return;
        }
        emit publishPressed();
    });

    d->updateState(State::Disconnected);
}

EncuadrePublishDialog::~EncuadrePublishDialog() = default;

void EncuadrePublishDialog::showDisconnected(const QString& _message)
{
    d->status->setText(_message);
    d->updateState(State::Disconnected);
}

void EncuadrePublishDialog::showWaitingForApproval()
{
    d->status->setText(tr("Se abrió el portal de Encuadre en tu navegador. Inicia sesión y "
                          "aprueba la conexión; esta ventana continuará sola."));
    d->updateState(State::WaitingForApproval);
}

void EncuadrePublishDialog::showReady(const QString& _account)
{
    d->status->setText(_account.isEmpty()
                           ? tr("Conectado al portal de Encuadre.")
                           : tr("Conectado al portal de Encuadre como %1.").arg(_account));
    d->updateState(State::Ready);
}

void EncuadrePublishDialog::showBusy(const QString& _message)
{
    d->status->setText(_message);
    d->updateState(State::Busy);
}

void EncuadrePublishDialog::showPublished(const QString& _message)
{
    d->status->setText(_message);
    d->updateState(State::Published);
    d->cancelButton->setText(tr("Cerrar"));
}

void EncuadrePublishDialog::showError(const QString& _message)
{
    d->status->setText(_message);
    d->updateState(d->state == State::Busy ? State::Ready : d->state);
}

void EncuadrePublishDialog::setProjects(const QVector<QPair<QString, QString>>& _projects,
                                        const QString& _selectedId)
{
    QSignalBlocker blocker(d->project);
    d->projectModel->clear();
    QModelIndex selected;
    for (const auto& project : _projects) {
        auto item = new QStandardItem(project.second);
        item->setData(project.first, kIdRole);
        d->projectModel->appendRow(item);
        if (project.first == _selectedId) {
            selected = item->index();
        }
    }
    if (!selected.isValid() && d->projectModel->rowCount() > 0) {
        selected = d->projectModel->index(0, 0);
    }
    d->project->setCurrentIndex(selected);
    d->updateState(d->state);
}

void EncuadrePublishDialog::setScripts(const QVector<QPair<QString, QString>>& _scripts,
                                       const QString& _selectedId, const QString& _newScriptName)
{
    QSignalBlocker blocker(d->script);
    d->scriptModel->clear();
    QModelIndex selected;
    for (const auto& script : _scripts) {
        auto item = new QStandardItem(script.second);
        item->setData(script.first, kIdRole);
        d->scriptModel->appendRow(item);
        if (script.first == _selectedId) {
            selected = item->index();
        }
    }
    auto newScriptItem = new QStandardItem(tr("Nuevo guion en el portal"));
    newScriptItem->setData(kNewScriptId, kIdRole);
    d->scriptModel->appendRow(newScriptItem);
    if (!selected.isValid()) {
        selected = d->scriptModel->index(0, 0);
    }
    d->script->setCurrentIndex(selected);
    d->newScriptName->setText(_newScriptName);
    d->updateState(d->state);
}

void EncuadrePublishDialog::setDrafts(const QStringList& _drafts)
{
    d->draftModel->clear();
    for (const auto& draft : _drafts) {
        d->draftModel->appendRow(new QStandardItem(draft));
    }
    if (d->draftModel->rowCount() > 0) {
        d->draft->setCurrentIndex(d->draftModel->index(0, 0));
    }
    d->updateState(d->state);
}

QString EncuadrePublishDialog::projectId() const
{
    return d->project->currentIndex().data(kIdRole).toString();
}

QString EncuadrePublishDialog::scriptId() const
{
    const auto id = d->script->currentIndex().data(kIdRole).toString();
    return id == kNewScriptId ? QString() : id;
}

QString EncuadrePublishDialog::newScriptName() const
{
    return d->newScriptName->text().trimmed();
}

int EncuadrePublishDialog::draftIndex() const
{
    return std::max(0, d->draft->currentIndex().row());
}

QString EncuadrePublishDialog::tag() const
{
    return d->tag->currentIndex().data(kIdRole).toString();
}

QString EncuadrePublishDialog::versionNumber() const
{
    return d->versionNumber->text().trimmed();
}

QString EncuadrePublishDialog::notes() const
{
    return d->notes->text().trimmed();
}

QWidget* EncuadrePublishDialog::focusedWidgetAfterShow() const
{
    return d->state == State::Disconnected ? static_cast<QWidget*>(d->connectButton)
                                           : static_cast<QWidget*>(d->project);
}

QWidget* EncuadrePublishDialog::lastFocusableWidget() const
{
    return d->publishButton;
}

void EncuadrePublishDialog::updateTranslations()
{
    setTitle(tr("Publicar guion en Encuadre"));

    d->project->setLabel(tr("Proyecto del portal"));
    d->script->setLabel(tr("Guion"));
    d->newScriptName->setLabel(tr("Nombre del nuevo guion"));
    d->draft->setLabel(tr("Borrador a publicar"));
    d->tag->setLabel(tr("Estado de la versión"));
    const QStringList tagNames = { tr("Borrador"), tr("En revisión"), tr("Aprobado"), tr("Final") };
    for (int row = 0; row < d->tagModel->rowCount() && row < tagNames.size(); ++row) {
        d->tagModel->item(row)->setText(tagNames.at(row));
    }
    d->versionNumber->setLabel(tr("Número de versión"));
    d->versionNumber->setHelper(tr("Déjalo vacío para numerarla automáticamente"));
    d->notes->setLabel(tr("Notas de la versión"));

    d->logoutButton->setText(tr("Cerrar sesión"));
    d->cancelButton->setText(d->state == State::Published ? tr("Cerrar") : tr("Cancelar"));
    d->connectButton->setText(tr("Conectar con Encuadre"));
    d->publishButton->setText(tr("Publicar"));
}

void EncuadrePublishDialog::designSystemChangeEvent(DesignSystemChangeEvent* _event)
{
    AbstractDialog::designSystemChangeEvent(_event);

    d->status->setBackgroundColor(DesignSystem::color().background());
    d->status->setTextColor(DesignSystem::color().onBackground());
    d->status->setContentsMargins(
        QMarginsF(DesignSystem::layout().px24(), DesignSystem::layout().px8(),
                  DesignSystem::layout().px24(), DesignSystem::layout().px12())
            .toMargins());

    for (auto textField : std::vector<TextField*>{
             d->project,
             d->script,
             d->newScriptName,
             d->draft,
             d->tag,
             d->versionNumber,
             d->notes,
         }) {
        textField->setTextColor(DesignSystem::color().onBackground());
        textField->setBackgroundColor(DesignSystem::color().onBackground());
        textField->setCustomMargins({ DesignSystem::layout().px24(), 0.0,
                                      DesignSystem::layout().px24(),
                                      DesignSystem::compactLayout().px16() });
    }
    for (auto combobox : { d->project, d->script, d->draft, d->tag }) {
        combobox->setPopupBackgroundColor(DesignSystem::color().background());
    }

    UiHelper::initColorsFor(d->logoutButton, UiHelper::DialogDefault);
    UiHelper::initColorsFor(d->cancelButton, UiHelper::DialogDefault);
    UiHelper::initColorsFor(d->connectButton, UiHelper::DialogAccept);
    UiHelper::initColorsFor(d->publishButton, UiHelper::DialogAccept);

    d->buttonsLayout->setContentsMargins(
        QMarginsF(DesignSystem::layout().px12(), DesignSystem::layout().px12(),
                  DesignSystem::layout().px16(), DesignSystem::layout().px12())
            .toMargins());
}

} // namespace Ui
