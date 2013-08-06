#ifndef COLORSPINBOXGROUP_H
#define COLORSPINBOXGROUP_H

#include <QWidget>

namespace Ui {
class ColorSpinBoxGroup;
}

class ColorSpinBoxGroup : public QWidget
{
    Q_OBJECT
    
public:
    explicit ColorSpinBoxGroup(QWidget *parent = 0);
    ~ColorSpinBoxGroup();
    QColor color();
signals:
    void colorChanged(const QColor& c);
    void modeChange(const bool& isRgb);

public slots:
    void setColor(const QColor &c);

private slots:
    void onModeChanged();
    void onColorChanged();
    
private:
    Ui::ColorSpinBoxGroup *ui;
    bool isRgbColors;
    bool noColorUpdate;
    QColor m_color;
};

#endif // COLORSPINBOXGROUP_H
