#pragma once

#include <QObject>

#include <functional>

class QWidget;


namespace ManagementLayer {

/**
 * @brief Управляющий публикацией сценариев в портал производства Encuadre
 *        (https://prod.encuadre.muxp.art)
 */
class EncuadreManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Данные о публикуемом проекте
     */
    struct PublishRequest {
        /**
         * @brief Путь к файлу проекта .encdre
         */
        QString projectFilePath;

        /**
         * @brief Название истории (используется для нового сценария в портале)
         */
        QString projectName;

        /**
         * @brief Названия сценариев (драфтов) проекта, доступных для публикации
         */
        QStringList draftNames;

        /**
         * @brief Экспортировать драфт с заданным индексом в PDF по заданному пути
         */
        std::function<bool(int, const QString&)> exportPdf;
    };

public:
    EncuadreManager(QObject* _parent, QWidget* _parentWidget);
    ~EncuadreManager() override;

    /**
     * @brief Показать диалог публикации сценария
     */
    void publish(const PublishRequest& _request);

private:
    class Implementation;
    QScopedPointer<Implementation> d;
};

} // namespace ManagementLayer
