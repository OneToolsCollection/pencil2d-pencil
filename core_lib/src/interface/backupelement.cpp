/*

Pencil - Traditional Animation Software
Copyright (C) 2005-2007 Patrick Corrieri & Pascal Naidon
Copyright (C) 2012-2018 Matthew Chiawen Chang

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#include "layermanager.h"
#include "backupmanager.h"
#include "viewmanager.h"

#include "layersound.h"
#include "layerbitmap.h"
#include "layervector.h"
#include "layercamera.h"

#include "editor.h"
#include "scribblearea.h"
#include "backupelement.h"

#include "vectorimage.h"
#include "bitmapimage.h"
#include "soundclip.h"
#include "camera.h"

BackupElement::BackupElement(Editor* editor, QUndoCommand* parent) : QUndoCommand(parent)
{
    qDebug() << "backupElement created";

    mEditor = editor;
}

BackupElement::~BackupElement()
{
}

AddKeyFrameElement::AddKeyFrameElement(int backupFrameIndex,
                                       int backupLayerId,
                                       int backupKeySpacing,
                                       bool backupKeyExisted,
                                       QString description,
                                       Editor *editor,
                                       QUndoCommand *parent) : BackupElement(editor, parent)
{

    newLayerIndex = editor->currentLayerIndex();
    newFrameIndex = editor->currentFrame();

    oldFrameIndex = backupFrameIndex;
    oldLayerId = backupLayerId;

    oldKeyExisted = backupKeyExisted;

    oldKeySpacing = backupKeySpacing;

    Layer* layer = editor->layers()->currentLayer();

    newKey = layer->getLastKeyFrameAtPosition(newFrameIndex)->clone();
    newLayerId = layer->id();

    oldKeyFrames.insert(std::make_pair(oldFrameIndex, newKey));

    bool isSequence = (oldKeySpacing > 1) ? true : false;
    switch(layer->type())
    {
        case Layer::BITMAP:
        {
            if (!description.isEmpty() || isSequence) { break; }
            description = "New Bitmap Key";
            break;
        }
        case Layer::VECTOR:
        {
            if (!description.isEmpty() || isSequence) { break; }

            description = "New Vector Key";
            break;
        }
        case Layer::SOUND:
        {
            if (!description.isEmpty() || isSequence) { break; }

            description = "New Sound Key";
            break;
        }
        case Layer::CAMERA:
        {
            if (!description.isEmpty() || isSequence) { break; }

            description = "New Camera Key";
            break;
        }
        default:
            break;
    }
    setText(QObject::tr(qPrintable(description)));
}

void AddKeyFrameElement::undo()
{
    qDebug() << "key remove triggered";
    bool isSequence = (oldKeySpacing > 1) ? true : false;
    if (isSequence)
    {
        qDebug() << "oldKeyFrames: " << oldKeyFrames;
        for (auto map : oldKeyFrames)
        {
            qDebug() << "did A key exist before:" << oldKeyExisted;
            if (!oldKeyExisted) {
                editor()->removeKeyAtLayerId(newLayerId, map.first);
            }
        }
    }
    else
    {
        editor()->removeKeyAtLayerId(newLayerId, newFrameIndex);
    }
    editor()->updateCurrentFrame();
}

void AddKeyFrameElement::redo()
{
    qDebug() << "undo: new backup frame " << newFrameIndex;
    qDebug() << "undo: newLayer" << newLayerIndex;

    if (isFirstRedo) { isFirstRedo = false; return; }
    bool isSequence = (oldKeySpacing > 1) ? true : false;

    if (newFrameIndex > 0)
    {
        if (isSequence)
        {
            qDebug() << "nnnew:" << newKeyFrames;
            for (auto map : newKeyFrames)
            {
                newFrameIndex = map.first; //
                newKey = map.second;
                editor()->backups()->restoreKey(this);
            }
        }
        else
        {
            qDebug() << "restore Addkey triggered";
            editor()->backups()->restoreKey(this);
        }
    }
    editor()->updateCurrentFrame();

}

bool AddKeyFrameElement::mergeWith(const QUndoCommand *other)
{
    qDebug() << "MERGE CHECK!";

    qDebug() << "state of frames: old" << oldKeyFrames;
    qDebug() << "state of frames:: new" << newKeyFrames;
    qDebug() << newKeyFrames;

    bool isSequence = (oldKeySpacing > 1) ? true : false;

    if (newKeyFrames.empty())
    {
        newKeyFrames.insert(std::make_pair(oldFrameIndex, newKey));
    }

    const AddKeyFrameElement* element = static_cast<const AddKeyFrameElement*>(other);

    if (!isSequence || element->oldKeySpacing < 2)
    {
        return false;
    }
    qDebug() << "MERGING!";

    oldFrameIndex = element->oldFrameIndex;
    newFrameIndex = element->newFrameIndex;
    newKey = element->newKey;
    oldKeyExisted = element->oldKeyExisted;

    std::map<int, KeyFrame*>frames = static_cast<const AddKeyFrameElement*>(other)->oldKeyFrames;
    for (auto map : frames)
    {
        oldKeyFrames.insert(std::make_pair(map.first, map.second));
    }

    newKeyFrames.insert(std::make_pair(oldFrameIndex, newKey));
    return true;
}


RemoveKeyFrameElement::RemoveKeyFrameElement(KeyFrame* backupKey,
                                             int backupLayerId,
                                             Editor *editor,
                                             QUndoCommand *parent) : BackupElement(editor, parent)
{

    newLayerIndex = editor->currentLayerIndex();
    newFrameIndex = editor->currentFrame();

    oldFrameIndex = backupKey->pos();

    oldLayerId = backupLayerId;
    newLayerId = editor->layers()->currentLayer()->id();

    Layer* layer = editor->layers()->findLayerById(newLayerId);

    oldKey = backupKey;

    switch(layer->type())
    {
        case Layer::BITMAP:
        {
            oldBitmap = static_cast<BitmapImage*>(backupKey);
            setText(QObject::tr("Remove Bitmap Key"));
            break;
        }
        case Layer::VECTOR:
        {
            oldVector = static_cast<VectorImage*>(backupKey);
            setText(QObject::tr("Remove Vector Key"));
            break;
        }
        case Layer::SOUND:
        {
            oldClip = static_cast<SoundClip*>(backupKey);
            setText(QObject::tr("Remove Sound Key"));
            break;
        }
        case Layer::CAMERA:
        {
            oldCamera = static_cast<Camera*>(backupKey);
            setText(QObject::tr("Remove Camera key"));
            break;
        }
        default:
            break;
    }
}

void RemoveKeyFrameElement::undo()
{
    Layer* layer = editor()->layers()->findLayerById(oldLayerId);

    qDebug() << "undo: new backup frame " << newFrameIndex;
    qDebug() << "undo: newLayer" << newLayerIndex;

    qDebug() << "undo: old frame index" << oldFrameIndex;

    if (layer->type() != Layer::SOUND)
    {

        qDebug() << "restore key";
        editor()->backups()->restoreKey(this);

    }
    else
    {
        editor()->removeKeyAtLayerId(oldLayerId, oldFrameIndex);
        editor()->backups()->restoreKey(this);
    }
}

void RemoveKeyFrameElement::redo()
{

    qDebug() << "redo: new backup frame " << newFrameIndex;
    qDebug() << "redo: old backup frame: " << oldFrameIndex;

    if (isFirstRedo) { isFirstRedo = false; return; }

    if (oldFrameIndex > 1)
    {
        qDebug() << "RemoveKeyFrame triggered";
        editor()->removeKeyAtLayerId(newLayerId, oldFrameIndex);
    }

}

bool RemoveKeyFrameElement::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id() || newFrameIndex != 1)
    {
        return false;
    }
    newFrameIndex = static_cast<const RemoveKeyFrameElement*>(other)->newFrameIndex;
    return true;
}


AddBitmapElement::AddBitmapElement(BitmapImage* backupBitmap,
                                   BitmapImage* backupBufferBitmap,
                                   int backupLayerId,
                                   int backupFrameIndex,
                                   QString description,
                                   Editor *editor,
                                   QUndoCommand *parent) : BackupElement(editor, parent)
{

    oldBitmap = backupBitmap->clone();
    oldBufferImage = backupBufferBitmap->clone();

    frameIndex = backupFrameIndex;
    newLayerIndex = editor->currentLayerIndex();
    oldLayerId = backupLayerId;

    Layer* layer = editor->layers()->currentLayer();
    newLayerId = layer->id();

    emptyFrameSettingVal = editor->preference()->
            getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);

    otherFrameIndex = frameIndex;
    if (emptyFrameSettingVal == DrawOnEmptyFrameAction::KEEP_DRAWING_ON_PREVIOUS_KEY)
    {
        previousFrameIndex = layer->getPreviousKeyFramePosition(frameIndex);
        otherFrameIndex = previousFrameIndex;
    }
    newBitmap = static_cast<LayerBitmap*>(layer)->
            getBitmapImageAtFrame(otherFrameIndex)->clone();

    setText(QObject::tr(qPrintable(description)));
}

void AddBitmapElement::undo()
{
    Layer* layer = editor()->layers()->findLayerById(oldLayerId);

    int framePos = frameIndex;
//    if (emptyFrameSettingVal == DrawOnEmptyFrameAction::KEEP_DRAWING_ON_PREVIOUS_KEY)
//    {
//        framePos = previousFrameIndex;
//    }

    if (editor()->getScribbleArea()->isSomethingSelected())
    {
        const TransformElement* childElem = static_cast<const TransformElement*>(this->child(0));
        ScribbleArea* scribbleArea = editor()->getScribbleArea();

        // clone old bitmap so we don't paint on the same...
        BitmapImage* oldBitmapClone = oldBitmap->clone();

        // get the previous image from our old iamge
        BitmapImage transformedImage = childElem->oldBitmap->transformed(childElem->oldSelectionRect.toRect(),
                                                                         childElem->oldTransform,
                                                                         false);

        // clear leftovers...
        oldBitmapClone->clear(childElem->oldSelectionRect);

        // paste tranformedImage to our cloned bitmap
        oldBitmapClone->paste(&transformedImage, QPainter::CompositionMode_SourceOver);

        // make the cloned bitmap the new canvas image.
        *static_cast<LayerBitmap*>(layer)->
                getBitmapImageAtFrame(framePos) = *oldBitmapClone;


        // set selections so the transform will be correct
        scribbleArea->mySelection = childElem->oldSelectionRectTemp;
        scribbleArea->myTempTransformedSelection = childElem->oldSelectionRectTemp;
        scribbleArea->myTransformedSelection= childElem->oldSelectionRectTemp;

        scribbleArea->paintTransformedSelection();
    }
    else
    {
        *static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(framePos) = *oldBitmap;
    }

    if (previousFrameIndex == frameIndex)
    {
        framePos = previousFrameIndex;
    }

    editor()->scrubTo(framePos);
    editor()->updateCurrentFrame();

}

void AddBitmapElement::redo()
{
    Layer* layer = editor()->layers()->findLayerById(newLayerId);

    int framePos = frameIndex;

    if (editor()->getScribbleArea()->isSomethingSelected())
    {
        applyToLastTransformedImage();
    }
    else
    {
        if (isFirstRedo)
        {
            isFirstRedo = false;
            return;
        }

        *static_cast<LayerBitmap*>(layer)->
                getBitmapImageAtFrame(otherFrameIndex) = *newBitmap;
    }

    if (previousFrameIndex == frameIndex)
    {
        framePos = previousFrameIndex;
    }

    editor()->scrubTo(framePos);
    editor()->updateCurrentFrame();

}

void AddBitmapElement::applyToLastTransformedImage()
{
    const TransformElement* childElem = static_cast<const TransformElement*>(this->child(0));
    ScribbleArea* scribbleArea = editor()->getScribbleArea();
    Layer* layer = editor()->layers()->findLayerById(newLayerId);

    newBitmap->clear(childElem->newSelectionRect);

    BitmapImage* newBitmapClone = newBitmap->clone();
    if (isFirstRedo) {

        // copy buffer image
        BitmapImage newTransformed = oldBufferImage->transformed(childElem->newSelectionRect.toRect(), QTransform(), false);

        // get transformed image from old position
        BitmapImage transformedImage = childElem->oldBitmap->transformed(childElem->newSelectionRect.toRect(), childElem->newTransform, false);

        // paste the old transformed image on new cloned bitmap
        newBitmapClone->paste(&transformedImage, QPainter::CompositionMode_SourceOver);

        // paste buffer image to make it look like the transformation has been applied
        newBitmapClone->paste(&newTransformed, QPainter::CompositionMode_SourceOver);
    }

    // set new bitmap to canvas (not painted yet)
    *static_cast<LayerBitmap*>(layer)->
            getBitmapImageAtFrame(otherFrameIndex) = *newBitmapClone;

    // reset transform and set selections
    scribbleArea->setSelectionTransform(QTransform());
    scribbleArea->mySelection = childElem->newSelectionRectTemp;
    scribbleArea->myTempTransformedSelection = childElem->newSelectionRectTemp;
    scribbleArea->myTransformedSelection= childElem->newSelectionRectTemp;

    // finally paint the transformation to apply to canvas
    scribbleArea->paintTransformedSelection();
}

AddVectorElement::AddVectorElement(VectorImage* backupVector, int backupLayerId, QString description, Editor* editor, QUndoCommand* parent) : BackupElement(editor, parent)
{

    oldVector = backupVector;
    newLayerIndex = editor->layers()->currentLayerIndex();
    frameIndex = editor->currentFrame();

    oldLayerId = backupLayerId;

    emptyFrameSettingVal = editor->preference()->
            getInt(SETTING::DRAW_ON_EMPTY_FRAME_ACTION);

    Layer* layer = editor->layers()->currentLayer();
    newLayerId = layer->id();

    otherFrameIndex = frameIndex;
    if (emptyFrameSettingVal == DrawOnEmptyFrameAction::KEEP_DRAWING_ON_PREVIOUS_KEY)
    {
        previousFrameIndex = layer->getPreviousKeyFramePosition(frameIndex);
        otherFrameIndex = previousFrameIndex;
    }
    newVector = static_cast<LayerVector*>(layer)->
            getVectorImageAtFrame(otherFrameIndex)->clone();

    setText(QObject::tr(qPrintable(description)));
}

void AddVectorElement::undo()
{
    qDebug() << "BackupVectorElement: undo";

    Layer* layer = editor()->layers()->findLayerById(newLayerId);

    int framePos = frameIndex;

    *static_cast<LayerVector*>(layer)->
            getVectorImageAtFrame(otherFrameIndex) = *oldVector;

    if (previousFrameIndex == frameIndex)
    {
        framePos = previousFrameIndex;
    }
    editor()->scrubTo(framePos);
    editor()->updateCurrentFrame();
}

void AddVectorElement::redo()
{
    qDebug() << "BackupVectorElement: redo";

    if (isFirstRedo) { isFirstRedo = false; return; }

    Layer* layer = editor()->layers()->findLayerById(newLayerId);
    int framePos = frameIndex;

    *static_cast<LayerVector*>(layer)->
            getVectorImageAtFrame(otherFrameIndex) = *newVector;

    if (previousFrameIndex == frameIndex)
    {
        framePos = previousFrameIndex;
    }
    editor()->scrubTo(framePos);
    editor()->updateCurrentFrame();

}

SelectionElement::SelectionElement(SelectionType backupSelectionType, QRectF backupTempSelection,
                                         QRectF backupSelection,
                                         Editor* editor,
                                         QUndoCommand* parent) : BackupElement(editor, parent)
{

    oldTempSelection = backupTempSelection;
    oldSelection = backupSelection;

    newTempSelection = editor->getScribbleArea()->myTempTransformedSelection;
    newSelection = editor->getScribbleArea()->mySelection;

    selectionType = backupSelectionType;

    if (selectionType == SelectionType::SELECTION) {
        setText(QObject::tr("New Selection"));
    } else {
        setText(QObject::tr("Deselected"));
    }

}

void SelectionElement::undo()
{
    if (selectionType == SelectionType::SELECTION) {
        undoSelection();
    } else {
        undoDeselection();
    }
}

void SelectionElement::undoSelection()
{
    ScribbleArea* scribbleArea = editor()->getScribbleArea();

    scribbleArea->myRotatedAngle = 0;
    scribbleArea->paintTransformedSelection();
    scribbleArea->deselectAll();

    editor()->updateCurrentFrame();
}

void SelectionElement::undoDeselection()
{
    ScribbleArea* scribbleArea = editor()->getScribbleArea();

    scribbleArea->mySelection = oldSelection;
    scribbleArea->myTempTransformedSelection = oldTempSelection;
    scribbleArea->myTransformedSelection = oldTempSelection;
    scribbleArea->trySelectSomething();

    const TransformElement* childElem = static_cast<const TransformElement*>(this->child(0));
    scribbleArea->setSelectionTransform(childElem->oldTransform);
    apply(childElem->oldLayerId,
          childElem->oldFrameIndex,
          childElem->oldBitmap,
          childElem->oldVector);

    scribbleArea->paintTransformedSelection();
}

void SelectionElement::apply(int layerId,
                             int frameIndex,
                             BitmapImage* bitmap,
                             VectorImage* vector)
{
    Layer* layer = editor()->layers()->findLayerById(layerId);
    Layer* currentLayer = editor()->layers()->currentLayer();

    if (currentLayer != layer) {
        editor()->layers()->setCurrentLayer(layer);
    }

    switch(layer->type())
    {
        case Layer::BITMAP:
        {
            *static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(frameIndex) = *bitmap;
            break;
        }
        case Layer::VECTOR:
        {
            *static_cast<LayerVector*>(layer)->getVectorImageAtFrame(frameIndex) = *vector;
        }
        default:
            break;
    }
}

void SelectionElement::redo()
{
    if (isFirstRedo) { isFirstRedo = false; return; }

    if (selectionType == SelectionType::SELECTION) {
        redoSelection();
    } else {
        redoDeselection();
    }

}

void SelectionElement::redoSelection()
{
    ScribbleArea* scribbleArea = editor()->getScribbleArea();
    scribbleArea->setSelection(newTempSelection);


    scribbleArea->myRotatedAngle = 0;
    scribbleArea->calculateSelectionTransformation();
    scribbleArea->paintTransformedSelection();

    editor()->updateCurrentFrame();
}

void SelectionElement::redoDeselection()
{
    ScribbleArea* scribbleArea = editor()->getScribbleArea();

    scribbleArea->mySelection = newSelection;
    scribbleArea->myTempTransformedSelection = newTempSelection;
    scribbleArea->myTransformedSelection = newTempSelection;
    scribbleArea->trySelectSomething();

    const TransformElement* childElem = static_cast<const TransformElement*>(this->child(0));
    scribbleArea->setSelectionTransform(childElem->newTransform);
    apply(childElem->newLayerId,
          childElem->newFrameIndex,
          childElem->newBitmap,
          childElem->newVector);

    scribbleArea->applyTransformedSelection();
    scribbleArea->paintTransformedSelection();

    scribbleArea->deselectAll();
}

bool SelectionElement::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id())
    {
        return false;
    }

    SelectionType otherType = static_cast<const SelectionElement*>(other)->selectionType;
    if (selectionType == SelectionType::SELECTION && otherType == selectionType) {
        newTempSelection = static_cast<const SelectionElement*>(other)->newTempSelection;
        newSelection = static_cast<const SelectionElement*>(other)->newSelection;

        ScribbleArea* scribbleArea = editor()->getScribbleArea();
        scribbleArea->mySelection = newSelection;
        scribbleArea->myTempTransformedMappedSelection = newTempSelection;
        scribbleArea->myTransformedSelection = newTempSelection;

        return true;
    } else {
        return false;
    }
}

TransformElement::TransformElement(KeyFrame* backupKeyFrame,
                                   BitmapImage* backupBufferImage,
                               int backupLayerId,
                               int backupFramePos,
                               QRectF backupSelection,
                               QRectF backupTempSelection,
                               QTransform backupTransform,
                               Editor *editor,
                               QUndoCommand *parent) : BackupElement(editor, parent)
{


    oldLayerId = backupLayerId;
    oldFrameIndex = backupFramePos;
    oldSelectionRect = backupSelection;
    oldSelectionRectTemp = backupTempSelection;
    oldTransform = backupTransform;
    bufferImg = backupBufferImage;

    Layer* newLayer = editor->layers()->currentLayer();
    newLayerId = newLayer->id();
    newFrameIndex = editor->currentFrame();
    newSelectionRect = editor->getScribbleArea()->mySelection;
    newSelectionRectTemp = editor->getScribbleArea()->myTempTransformedSelection;
    newTransform = editor->getScribbleArea()->getSelectionTransformation();

//    qDebug() << "old T:" << oldTransform;
//    qDebug() << "newT:" << newTransform;

    Layer* layer = editor->layers()->findLayerById(backupLayerId);
    switch(layer->type())
    {
        case Layer::BITMAP:
        {
            oldBitmap = static_cast<BitmapImage*>(backupKeyFrame);
            newBitmap = static_cast<LayerBitmap*>(layer)->getBitmapImageAtFrame(newFrameIndex)->clone();
            break;
        }
        case Layer::VECTOR:
        {
            oldVector = static_cast<VectorImage*>(backupKeyFrame);
            newVector = static_cast<LayerVector*>(layer)->
                    getVectorImageAtFrame(backupFramePos)->clone();
            break;
        }
        default:
            break;
    }

//    ScribbleArea* scribbleArea = editor->getScribbleArea();

//    scribbleArea->mySelection = newSelectionRect;
//    scribbleArea->paintTransformedSelection();


    setText("Moved Image");
}

void TransformElement::undo()
{
    apply(oldSelectionRectTemp,
          oldBitmap,
          oldVector,
          oldSelectionRect,
          oldTransform,
          oldFrameIndex,
          oldLayerId);
}

void TransformElement::redo()
{
    if (isFirstRedo) {
        isFirstRedo = false; return;
    }

    apply(newSelectionRectTemp,
          newBitmap,
          newVector,
          newSelectionRect,
          newTransform,
          newFrameIndex,
          newLayerId);
}

void TransformElement::apply(QRectF tempRect,
                             BitmapImage* bitmapImage,
                             VectorImage* vectorImage,
                             QRectF selectionRect,
                             QTransform transform,
                             int frameIndex,
                             int layerId)
{
    ScribbleArea* scribbleArea = editor()->getScribbleArea();

    Layer* layer = editor()->layers()->findLayerById(layerId);
    Layer* currentLayer = editor()->layers()->currentLayer();

    if (layer->type() != currentLayer->type())
    {
        editor()->layers()->setCurrentLayer(layer);
    }

    scribbleArea->setSelectionTransform(transform);
    scribbleArea->mySelection = selectionRect;
    scribbleArea->myTempTransformedSelection = tempRect;
    scribbleArea->myTransformedSelection = tempRect;
    scribbleArea->trySelectSomething();

    switch(layer->type())
    {
        case Layer::BITMAP:
        {
            *static_cast<LayerBitmap*>(layer)->getLastBitmapImageAtFrame(frameIndex) = *bitmapImage;
            break;
        }
        case Layer::VECTOR:
        {
            *static_cast<LayerVector*>(layer)->getVectorImageAtFrame(frameIndex) = *vectorImage;
            break;
        }
        default:
            break;

    }
    editor()->getScribbleArea()->paintTransformedSelection();
}

bool TransformElement::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id())
    {
        return false;
    }
    const TransformElement* elem = static_cast<const TransformElement*>(other);

    newSelectionRectTemp = elem->newSelectionRectTemp;
    newBitmap = elem->newBitmap;
    newVector = elem->newVector;
    newSelectionRect = elem->newSelectionRect;
    newTransform = elem->newTransform;
    newFrameIndex = elem->newFrameIndex;
    newLayerId = elem->newLayerId;

    apply(newSelectionRectTemp,
          newBitmap,
          newVector,
          newSelectionRect,
          newTransform,
          newFrameIndex,
          newLayerId);

    ScribbleArea* scribbleArea = editor()->getScribbleArea();

    scribbleArea->mySelection =  newSelectionRect;
    scribbleArea->myTempTransformedSelection = newSelectionRectTemp;
    scribbleArea->myTransformedSelection = newSelectionRectTemp;

    scribbleArea->paintTransformedSelection();

    return true;
}

CameraMotionElement::CameraMotionElement(QPointF backupTranslation,
                                         float backupRotation,
                                         float backupScale,
                                         Editor* editor,
                                         QUndoCommand* parent) : BackupElement(editor, parent)
{

    oldTranslation = backupTranslation;
    oldRotation = backupRotation;
    oldScale = backupScale;

    ViewManager* viewMgr = editor->view();
    newTranslation = viewMgr->translation();
    newRotation = viewMgr->rotation();
    newScale = viewMgr->scaling();

    setText(QObject::tr("New Camera Motion"));

}

void CameraMotionElement::undo()
{
    ViewManager* viewMgr = editor()->view();

    viewMgr->translate(oldTranslation);
    viewMgr->rotate(oldRotation);
    viewMgr->scale(oldScale);
}

void CameraMotionElement::redo()
{
    if (isFirstRedo) { isFirstRedo = false; return; }

    ViewManager* viewMgr = editor()->view();
    viewMgr->translate(newTranslation);
    viewMgr->rotate(newRotation);
    viewMgr->scale(newScale);

}

bool CameraMotionElement::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id())
    {
        return false;
    }
    newTranslation = static_cast<const CameraMotionElement*>(other)->newTranslation;
    newRotation = static_cast<const CameraMotionElement*>(other)->newRotation;
    newScale = static_cast<const CameraMotionElement*>(other)->newScale;
    return true;
}

AddLayerElement::AddLayerElement(Layer* backupLayer,
                                 Editor* editor,
                                 QUndoCommand* parent) : BackupElement(editor, parent)
{


    oldLayer = backupLayer;
    oldLayerId = backupLayer->id();

    Layer* layer = editor->layers()->currentLayer();
    newLayerType = layer->type();
    newLayerId = layer->id();

    switch(layer->type())
    {
        case Layer::BITMAP:
        {
            newLayer = new LayerBitmap(layer->object());
            break;
        }
        case Layer::VECTOR:
        {
            newLayer = new LayerVector(layer->object());
            break;
        }
        case Layer::SOUND:
        {
            newLayer = new LayerSound(layer->object());
            break;
        }
        case Layer::CAMERA:
        {
            newLayer = new LayerCamera(layer->object());
            break;
        }
        default:
            Q_ASSERT(false);
    }
    newLayerName = layer->name();


    setText(QObject::tr("New Layer"));
}

void AddLayerElement::undo()
{
    qDebug() << "undo";
    qDebug() << "oldLayerId:" << oldLayerId;
    qDebug() << "newLayerId:" << newLayerId;
    editor()->layers()->deleteLayerWithId(newLayerId, newLayerType);

}

void AddLayerElement::redo()
{
    if (isFirstRedo) { isFirstRedo = false; return; }

    switch(newLayer->type())
    {
        case Layer::BITMAP:
        {
            editor()->layers()->createBitmapLayer(newLayerName);
            break;
        }
        case Layer::VECTOR:
        {
            editor()->layers()->createVectorLayer(newLayerName);
            break;
        }
        case Layer::SOUND:
        {
            editor()->layers()->createSoundLayer(newLayerName);
            break;
        }
        case Layer::CAMERA:
        {
            editor()->layers()->createCameraLayer(newLayerName);
            break;
        }
        default:
            break;
    }

}

DeleteLayerElement::DeleteLayerElement(QString backupLayerName,
                                       Layer::LAYER_TYPE backupType,
                                       std::map<int, KeyFrame*, std::greater<int> > backupLayerKeys,
                                       int backupLayerIndex,
                                       int backupLayerId,
                                       Editor* editor,
                                       QUndoCommand* parent) : BackupElement(editor, parent)
{


    oldLayerIndex = backupLayerIndex;
    oldLayerName = backupLayerName;
    oldLayerKeys = backupLayerKeys;
    oldLayerType = backupType;
    oldLayerId = backupLayerId;
    oldFrameIndex = editor->currentFrame();

    newLayerIndex = editor->currentLayerIndex();
    Layer* layer = editor->layers()->currentLayer();
    newLayerId = layer->id();

    switch(oldLayerType)
    {
        case Layer::BITMAP:
        {
            setText(QObject::tr("Delete Bitmap Layer"));
            break;
        }
        case Layer::VECTOR:
        {
            setText(QObject::tr("Delete Vector Layer"));
            break;
        }
        case Layer::SOUND:
        {
            setText(QObject::tr("Delete Sound Layer"));
            break;
        }
        case Layer::CAMERA:
        {
            setText(QObject::tr("Delete Camera Layer"));
            break;
        }
        default:
            break;
    }
}

void DeleteLayerElement::undo()
{
    editor()->backups()->restoreLayerKeys(this);
}

void DeleteLayerElement::redo()
{

    qDebug() << "layer id " << oldLayerId;
    qDebug() << "new layer id " << newLayerId;
    qDebug() << "old LayerIndex" << oldLayerIndex;

    if (isFirstRedo) { isFirstRedo = false; return; }

    qDebug() << "layer remove triggered";
    editor()->layers()->deleteLayerWithId(oldLayerId, oldLayerType);

}

RenameLayerElement::RenameLayerElement(QString backupLayerName, int backupLayerId, Editor *editor, QUndoCommand *parent) : BackupElement(editor, parent)
{

    oldLayerName = backupLayerName;
    oldLayerId = backupLayerId;

    Layer* layer = editor->layers()->currentLayer();
    newLayerId = layer->id();
    newLayerName = layer->name();

    setText(QObject::tr("Rename layer"));
}

void RenameLayerElement::undo()
{
    Layer* layer = editor()->layers()->findLayerById(oldLayerId);
    editor()->layers()->renameLayer(layer, oldLayerName);
}

void RenameLayerElement::redo()
{
    if (isFirstRedo) { isFirstRedo = false; return; }
    Layer* layer = editor()->layers()->findLayerById(newLayerId);
    editor()->layers()->renameLayer(layer, newLayerName);

}

CameraPropertiesElement::CameraPropertiesElement(QString backupLayerName,
                                                         QRect backupViewRect,
                                                         int backupLayerId,
                                                         Editor *editor,
                                                         QUndoCommand *parent) : BackupElement(editor, parent)
{

    oldLayerId = backupLayerId;
    oldViewRect = backupViewRect;
    oldLayerName = backupLayerName;

    LayerCamera* layer = static_cast<LayerCamera*>(editor->layers()->currentLayer());

    newLayerId = layer->id();
    newLayerName = layer->name();
    newViewRect = layer->getViewRect();

    if (oldViewRect != newViewRect)
    {
        setText(QObject::tr("Edit Camera Properties"));
    }
    else
    {
        setText(QObject::tr("Rename Layer"));
    }

}

void CameraPropertiesElement::undo()
{
    LayerManager* lmgr = editor()->layers();
    LayerCamera* layer = static_cast<LayerCamera*>(lmgr->findLayerById(oldLayerId));

    lmgr->renameLayer(layer, oldLayerName);
    layer->setViewRect(oldViewRect);
    editor()->updateCurrentFrame();

}

void CameraPropertiesElement::redo()
{

    if (isFirstRedo) { isFirstRedo = false; return; }

    LayerManager* lmgr = editor()->layers();
    LayerCamera* layer = static_cast<LayerCamera*>(lmgr->findLayerById(newLayerId));

    if (layer->name() != newLayerName)
    {
        lmgr->renameLayer(layer, newLayerName);
    }
    if (layer->getViewRect() != newViewRect)
    {
        layer->setViewRect(newViewRect);
    }
    editor()->updateCurrentFrame();
}

DragFrameElement::DragFrameElement(int backupLayerId,
                                   int backupFrameOffset,
                                   Editor* editor,
                                   QUndoCommand* parent) : BackupElement(editor, parent)
{

    layerId = backupLayerId;
    frameOffset = backupFrameOffset;
    startFrame = editor->layers()->findLayerById(backupLayerId)->getFirstFrameInSelection();
    endFrame = editor->layers()->findLayerById(backupLayerId)->getLastFrameInSelection();

    if (startFrame == endFrame)
    {
        setText(QObject::tr("Move Frame"));
    }
    else
    {
        setText(QObject::tr("Move Frames"));
    }
}

void DragFrameElement::undo()
{

    qDebug() << "UNDO";
    qDebug() << "start: " << startFrame;
    qDebug() << "end: " << endFrame;
    Layer* layer = editor()->layers()->findLayerById(layerId);

    layer->deselectAll();
    layer->setFrameSelected(endFrame, true);
    layer->extendSelectionTo(startFrame);
    layer->moveSelectedFrames(-frameOffset);

    editor()->updateTimeLine();
    editor()->updateCurrentFrame();
}

void DragFrameElement::redo()
{

    if (isFirstRedo) { isFirstRedo = false; return; }

    Layer* layer = editor()->layers()->findLayerById(layerId);
    qDebug() << "REDO";
    qDebug() << "start: " << startFrame;
    qDebug() << "end: " << endFrame;

    layer->deselectAll();
    layer->setFrameSelected(endFrame-frameOffset, true);
    layer->extendSelectionTo(startFrame-frameOffset);
    layer->moveSelectedFrames(frameOffset);

    editor()->updateTimeLine();
    editor()->updateCurrentFrame();
}

FlipViewElement::FlipViewElement(bool backupFlipState, DIRECTION backupFlipDirection, Editor *editor, QUndoCommand *parent) : BackupElement(editor, parent)
{


    isFlipped = backupFlipState;
    direction = backupFlipDirection;

    if (direction == DIRECTION::HORIZONTAL)
    {
        setText(QObject::tr("Flip View X"));
    }
    else
    {
        setText(QObject::tr("Flip View Y"));
    }
}

void FlipViewElement::undo()
{
    if (direction == DIRECTION::VERTICAL)
    {
        editor()->view()->flipVertical(!isFlipped);
    }
    else
    {
        editor()->view()->flipHorizontal(!isFlipped);
    }
}

void FlipViewElement::redo()
{

    if (isFirstRedo) { isFirstRedo = false; return; }

    if (direction == DIRECTION::VERTICAL)
    {
        editor()->view()->flipVertical(isFlipped);
    }
    else
    {
        editor()->view()->flipHorizontal(isFlipped);
    }
}

MoveLayerElement::MoveLayerElement(int backupOldLayerIndex, int backupNewLayerIndex, Editor* editor, QUndoCommand* parent) : BackupElement(editor, parent)
{


    oldLayerIndex = backupOldLayerIndex;
    newLayerIndex = backupNewLayerIndex;
    qDebug() << newLayerIndex;
    qDebug() << editor->currentLayerIndex();

    setText(QObject::tr("Move layer"));
}

void MoveLayerElement::undo()
{
    editor()->moveLayer(newLayerIndex, oldLayerIndex);
}

void MoveLayerElement::redo()
{
    if (isFirstRedo) { isFirstRedo = false; return; }

    editor()->moveLayer(oldLayerIndex, newLayerIndex);

}



