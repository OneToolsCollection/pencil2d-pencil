/*

Pencil2D - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2020 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/
#include "layersound.h"

#include <QDebug>
#include <QMediaPlayer>
#include <QFileInfo>
#include <QDir>
#include "soundclip.h"
#include "util/util.h"


LayerSound::LayerSound(int id) : Layer(id, Layer::SOUND)
{
    setName(tr("Sound Layer"));
}

LayerSound::~LayerSound()
{
}

Status LayerSound::loadSoundClipAtFrame(const QString& sSoundClipName,
                                        const QString& strFilePath,
                                        int frameNumber)
{
    if (!QFile::exists(strFilePath))
    {
        return Status::FILE_NOT_FOUND;
    }

    QFileInfo info(strFilePath);
    if (!info.isFile())
    {
        return Status::ERROR_LOAD_SOUND_FILE;
    }

    SoundClip* clip = new SoundClip;
    clip->setSoundClipName(sSoundClipName);
    clip->init(strFilePath);
    clip->setPos(frameNumber);
    loadKey(clip);
    return Status::OK;
}

void LayerSound::updateFrameLengths(int fps)
{
    foreachKeyFrame([&fps](KeyFrame* pKeyFrame)
    {
        auto soundClip = dynamic_cast<SoundClip *>(pKeyFrame);
        soundClip->updateLength(fps);
    });
}

QDomElement LayerSound::createDomElement(QDomDocument& doc) const
{
    QDomElement layerElem = createBaseDomElement(doc);

    foreachKeyFrame([&doc, &layerElem](KeyFrame* pKeyFrame)
    {
        SoundClip* clip = static_cast<SoundClip*>(pKeyFrame);

        QDomElement imageTag = doc.createElement("sound");
        imageTag.setAttribute("frame", clip->pos());
        imageTag.setAttribute("name", clip->soundClipName());

        QFileInfo info(clip->fileName());
        //qDebug() << "Save=" << info.fileName();
        imageTag.setAttribute("src", info.fileName());
        layerElem.appendChild(imageTag);
    });

    return layerElem;
}

void LayerSound::loadDomElement(const QDomElement& element, QString dataDirPath, ProgressCallback progressStep)
{
    this->loadBaseDomElement(element);

    QDomNode soundTag = element.firstChild();
    while (!soundTag.isNull())
    {
        QDomElement soundElement = soundTag.toElement();

        if (!soundElement.isNull() && soundElement.tagName() == "sound")
        {
            const QString soundFile = soundElement.attribute("src");
            const QString sSoundClipName = soundElement.attribute("name", "My Sound Clip");

            if (!soundFile.isEmpty())
            {
                QString path = validateDataPath(soundFile, dataDirPath);
                if (!path.isEmpty())
                {
                    int position = soundElement.attribute("frame").toInt();
                    Status st = loadSoundClipAtFrame(sSoundClipName, path, position);
                    Q_ASSERT(st.ok());
                }
            }
            progressStep();
        }

        soundTag = soundTag.nextSibling();
    }
}

void LayerSound::replaceKeyFrame(const KeyFrame* soundClip)
{
    *getSoundClipWhichCovers(soundClip->pos()) = *static_cast<const SoundClip*>(soundClip);
}

Status LayerSound::saveKeyFrameFile(KeyFrame* key, QString path)
{
    Q_ASSERT(key);

    if (key->fileName().isEmpty())
    {
        return Status::SAFE;
    }

    QFileInfo info(key->fileName());
    QString sDestFileLocation = QDir(path).filePath(info.fileName());

    if (sDestFileLocation != key->fileName())
    {
        if (QFile::exists(sDestFileLocation))
            QFile::remove(sDestFileLocation);

        bool ok = QFile::copy(key->fileName(), sDestFileLocation);
        if (!ok)
        {
            key->setFileName("");

            DebugDetails dd;
            dd << "LayerSound::saveKeyFrameFile";
            dd << QString("  KeyFrame.pos() = %1").arg(key->pos());
            dd << QString("  Key->fileName() = %1").arg(key->fileName());
            dd << QString("  FilePath = %1").arg(sDestFileLocation);
            dd << QString("Error: Failed to save SoundClip");
            return Status(Status::FAIL, dd);
        }
        key->setFileName(sDestFileLocation);
    }
    return Status::OK;
}

KeyFrame* LayerSound::createKeyFrame(int position)
{
    SoundClip* s = new SoundClip;
    s->setPos(position);
    return s;
}

SoundClip* LayerSound::getSoundClipWhichCovers(int frameNumber)
{
    KeyFrame* key = getKeyFrameWhichCovers(frameNumber);
    return static_cast<SoundClip*>(key);
}
