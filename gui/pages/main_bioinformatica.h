#pragma once
#include <QWidget>
class AiClient;

class BioinformaticaPage : public QWidget {
    Q_OBJECT
public:
    explicit BioinformaticaPage(AiClient* ai, QWidget* parent = nullptr);
};
