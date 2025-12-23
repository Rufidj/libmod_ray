#include "texturepalette.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>

TexturePalette::TexturePalette(QWidget *parent)
    : QWidget(parent)
    , m_selectedTexture(1)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *label = new QLabel("Texturas:", this);
    layout->addWidget(label);
    
    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setIconSize(QSize(64, 64));
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setMovement(QListWidget::Static);
    
    layout->addWidget(m_listWidget);
    
    connect(m_listWidget, &QListWidget::itemClicked, this, &TexturePalette::onItemClicked);
}

void TexturePalette::setTextures(const QVector<TextureEntry> &textures)
{
    m_textureMap.clear();
    m_listWidget->clear();
    
    for (const TextureEntry &tex : textures) {
        m_textureMap[tex.id] = tex.pixmap;
        
        QListWidgetItem *item = new QListWidgetItem(m_listWidget);
        item->setIcon(QIcon(tex.pixmap));
        item->setText(QString::number(tex.id));
        item->setData(Qt::UserRole, tex.id);
        item->setToolTip(QString("ID: %1").arg(tex.id));
    }
    
    qDebug() << "Texturas cargadas en paleta:" << m_textureMap.size();
}

int TexturePalette::getSelectedTexture() const
{
    return m_selectedTexture;
}

void TexturePalette::onItemClicked(QListWidgetItem *item)
{
    if (item) {
        m_selectedTexture = item->data(Qt::UserRole).toInt();
        qDebug() << "Textura seleccionada:" << m_selectedTexture;
        emit textureSelected(m_selectedTexture);
    }
}
