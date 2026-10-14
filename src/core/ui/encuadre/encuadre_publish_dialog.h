#pragma once

#include <ui/widgets/dialog/abstract_dialog.h>


namespace Ui {

/**
 * @brief Диалог публикации сценария в портал производства Encuadre
 */
class EncuadrePublishDialog : public AbstractDialog
{
    Q_OBJECT

public:
    explicit EncuadrePublishDialog(QWidget* _parent = nullptr);
    ~EncuadrePublishDialog() override;

    /**
     * @brief Состояния диалога
     */
    void showDisconnected(const QString& _message);
    void showWaitingForApproval();
    void showReady(const QString& _account);
    void showBusy(const QString& _message);
    void showPublished(const QString& _message);
    void showError(const QString& _message);

    /**
     * @brief Списки проектов, сценариев портала и драфтов локального проекта
     *        (пары идентификатор - название)
     */
    void setProjects(const QVector<QPair<QString, QString>>& _projects,
                     const QString& _selectedId);
    void setScripts(const QVector<QPair<QString, QString>>& _scripts, const QString& _selectedId,
                    const QString& _newScriptName);
    void setDrafts(const QStringList& _drafts);

    /**
     * @brief Выбранные параметры публикации
     */
    QString projectId() const;
    QString scriptId() const;
    QString newScriptName() const;
    int draftIndex() const;
    QString tag() const;
    QString versionNumber() const;
    QString notes() const;

signals:
    void connectPressed();
    void logoutPressed();
    void projectSelected(const QString& _projectId);
    void publishPressed();
    void cancelPressed();

protected:
    QWidget* focusedWidgetAfterShow() const override;
    QWidget* lastFocusableWidget() const override;

    void updateTranslations() override;
    void designSystemChangeEvent(DesignSystemChangeEvent* _event) override;

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace Ui
