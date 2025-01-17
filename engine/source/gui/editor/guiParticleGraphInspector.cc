//-----------------------------------------------------------------------------
// Copyright (c) 2013 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/consoleTypes.h"
#include "graphics/dgl.h"
#include "gui/guiDefaultControlRender.h"
#include "math/rectClipper.h"
#include "gui/guiCanvas.h"

#ifndef _PARTICLE_ASSET_H_
#include "2d/assets/ParticleAsset.h"
#endif

#include "gui/editor/guiParticleGraphInspector.h"

#include "gui/editor/guiParticleGraphInspector_ScriptBinding.h"

IMPLEMENT_CONOBJECT(GuiParticleGraphInspector);

GuiParticleGraphInspector::GuiParticleGraphInspector()
{
	mBounds.extent.set(300, 200);

	mTargetAsset = NULL;
	mTargetField = StringTable->insert("QuantityScale");
	mEmitterIndex = 0;
	mVariationInspector = NULL;
	mMinX = 0;
	mMinXLabel = StringTable->insert("0");
	mMaxX = 1;
	mMaxXLabel = StringTable->insert("1");
	mMinY = 0;
	mMinYLabel = StringTable->insert("0");
	mMaxY = 10;
	mMaxYLabel = StringTable->insert("10");
	mLabelX = StringTable->insert("Time", true);
	mLabelY = StringTable->insert("Value", true);
	mSelectedIndex = -1;
	mDirty = true;
	mPointList = Vector<GraphPoint>();

	setField("profile", "GuiDefaultProfile");
}

void GuiParticleGraphInspector::initPersistFields()
{
	Parent::initPersistFields();
}

void GuiParticleGraphInspector::inspectObject(ParticleAsset* object)
{
	mTargetAsset = object;
	mDirty = true;
}

void GuiParticleGraphInspector::setDisplayField(const char* fieldName)
{
	if (mTargetField != StringTable->insert(fieldName, true))
	{
		mSelectedIndex = -1;
	}
	mTargetField = StringTable->insert(fieldName, true);

	mDirty = true;
}

void GuiParticleGraphInspector::setDisplayField(const char* fieldName, U16 index)
{
	mEmitterIndex = index;
	setDisplayField(fieldName);
}

void GuiParticleGraphInspector::setDisplayArea(StringTableEntry minX, StringTableEntry minY, StringTableEntry maxX, StringTableEntry maxY)
{
	mMinXLabel = minX;
	mMinX = dAtof(minX);

	mMinYLabel = minY;
	mMinY = dAtof(minY);

	mMaxXLabel = maxX;
	mMaxX = dAtof(maxX);

	mMaxYLabel = maxY;
	mMaxY = dAtof(maxY);

	mDirty = true;
}

void GuiParticleGraphInspector::setDisplayLabels(const char* labelX, const char* labelY)
{
	mLabelX = StringTable->insert(labelX, true);
	mLabelY = StringTable->insert(labelY, true);
}

ParticleAssetField* GuiParticleGraphInspector::getTargetField()
{
	ParticleAssetFieldCollection& collection = mTargetAsset->getParticleFields();
	ParticleAssetField* field = collection.findField(mTargetField);

	if (field == NULL)
	{
		if (mEmitterIndex >= mTargetAsset->getEmitterCount())
		{
			mEmitterIndex = mTargetAsset->getEmitterCount() - 1;
		}

		ParticleAssetEmitter* emitter = mTargetAsset->getEmitter(mEmitterIndex);
		ParticleAssetFieldCollection& emitterCollection = emitter->getParticleFields();
		field = emitterCollection.findField(mTargetField);
	}

	AssertFatal(field != NULL, "GuiParticleGraphInspector::getTargetField() - Unable to find the requested field.");

	return field;
}

void GuiParticleGraphInspector::resize(const Point2I &newPosition, const Point2I &newExtent)
{
	GuiControl::resize(newPosition, newExtent);
	mDirty = true;
}

void GuiParticleGraphInspector::setControlProfile(GuiControlProfile *prof)
{
	GuiControl::setControlProfile(prof);
	mDirty = true;
}

void GuiParticleGraphInspector::onTouchUp(const GuiEvent &event)
{
	if(mTargetAsset)
		mTargetAsset->refreshAsset();
}

void GuiParticleGraphInspector::onTouchDown(const GuiEvent &event)
{
	if(!mTargetAsset)
		return;

	mSelectedIndex = findHitGraphPoint(event.mousePoint);

	if (mSelectedIndex != -1 && event.mouseClickCount == 2)
	{
		//remove the point
		ParticleAssetField* field = getTargetField();
		field->removeDataKey(mSelectedIndex);

		mDirty = true;
	}
	else if (mSelectedIndex == -1 && mGridRect.pointInRect(event.mousePoint))
	{
		//Time to create a new point!
		ParticleAssetField* field = getTargetField();
		F32 time = getGraphTime(event.mousePoint.x);
		F32 value = getGraphValue(event.mousePoint.y);
		mSelectedIndex = field->addDataKey(time, value);

		mDirty = true;
	}
}

void GuiParticleGraphInspector::onTouchDragged(const GuiEvent &event)
{
	if (!mTargetAsset)
		return;

	Point2I point = Point2I(mClamp(event.mousePoint.x, mGridRect.point.x, mGridRect.point.x + mGridRect.extent.x), mClamp(event.mousePoint.y, mGridRect.point.y, mGridRect.point.y + mGridRect.extent.y));

	if (mSelectedIndex == 0)
	{
		//Time to move the first point!
		ParticleAssetField* field = getTargetField();
		F32 value = getGraphValue(point.y);
		field->setDataKeyValue(mSelectedIndex, value);

		mDirty = true;
	}
	else if (mSelectedIndex > 0)
	{
		//Time to move a point!
		ParticleAssetField* field = getTargetField();
		F32 time = getGraphTime(point.x);
		F32 value = getGraphValue(point.y);
		if (time == field->getDataKeyTime(mSelectedIndex) || field->doesKeyExist(time))
		{
			//If we're not moving through time or we tried to drag it into a time with a different point, then just change the value.
			field->setDataKeyValue(mSelectedIndex, value);
		}
		else
		{
			//Time travel! Destroy the old point. Recreate in a new time.
			field->removeDataKey(mSelectedIndex);
			mSelectedIndex = field->addDataKey(time, value);
		}

		mDirty = true;
	}
}

U32 GuiParticleGraphInspector::findHitGraphPoint(const Point2I &point)
{
	for (U32 i = 0; i < mPointList.size(); i++)
	{
		F32 x = mPointList[i].mPoint.x - point.x;
		F32 y = mPointList[i].mPoint.y - point.y;
		F32 dist = mSqrt((x * x) + (y * y));
		if (dist <= mRadius)
		{
			return i;
		}
	}
	return -1;
}

F32 GuiParticleGraphInspector::getGraphValue(const F32 y)
{
	S32 len = mGridRect.extent.y;
	F32 ratio = (F32)((y - mGridRect.point.y) / len);
	ratio = mRound(ratio * 100) / 100; //Snaps to a grid of 100 possible values.
	return mMinY + ((mMaxY - mMinY) * (1 - ratio));
}

F32 GuiParticleGraphInspector::getGraphTime(const F32 x)
{
	S32 len = mGridRect.extent.x;
	F32 ratio = (F32)((x - mGridRect.point.x) / len);
	ratio = mRound(ratio * 100) / 100; //Snaps to a grid of 100 possible values.
	return mMinX + ((mMaxX - mMinX) * ratio);
}

void GuiParticleGraphInspector::onRender(Point2I offset, const RectI &updateRect)
{
	RectI ctrlRect = applyMargins(offset, mBounds.extent, NormalState, mProfile);
	if (!ctrlRect.isValidRect())
	{
		return;
	}
	renderUniversalRect(ctrlRect, mProfile, NormalState);

	RectI fillRect = applyBorders(ctrlRect.point, ctrlRect.extent, NormalState, mProfile);
	RectI contentRect = applyPadding(fillRect.point, fillRect.extent, NormalState, mProfile);

	//Make room for the graph labels
	GFont *font = mProfile->getFont(mFontSizeAdjust);
	U32 fontHeight = font->getHeight();
	contentRect.extent.y -= fontHeight;
	U8 xReduction = getMax(getMax(fontHeight, font->getStrWidth(mMaxYLabel)), font->getStrWidth(mMinYLabel));
	contentRect.extent.x -= xReduction;
	contentRect.point.x += xReduction;

	//reduce the contentRect to be divisible by 10
	U32 modX = contentRect.len_x() % 10;
	U32 modY = contentRect.len_y() % 10;
	contentRect.extent.set(contentRect.len_x() - modX, contentRect.len_y() - modY);
	contentRect.point.set(contentRect.point.x + mFloor(modX / 2), contentRect.point.y + mFloor(modY / 2));

	//Draw the labels
	ColorI gridColor = mProfile->getFillColor(HighlightState);
	renderLabels(contentRect, gridColor);

	if (contentRect.isValidRect())
	{
		renderGrid(contentRect, gridColor);

		if (mCalculationOffset != offset)
		{
			mDirty = true;
		}
		renderPoints(contentRect, mProfile->getFillColor(SelectedState));
		mCalculationOffset = offset;
	}
}

void GuiParticleGraphInspector::renderLabels(const RectI &contentRect, const ColorI &labelColor)
{
	GFont *font = mProfile->getFont(mFontSizeAdjust);
	U32 fontHeight = font->getHeight();
	U32 textWidth;
	Point2I textPoint;

	//Set the color used for the grid. This will also be used for the text.
	dglSetBitmapModulation(labelColor);

	//x label
	textWidth = font->getStrWidth(mLabelX);
	textPoint = Point2I(contentRect.point.x + (contentRect.extent.x / 2) - (textWidth / 2), contentRect.point.y + contentRect.extent.y + 2);
	dglDrawText(font, textPoint, mLabelX, NULL, 0, 0);

	//x min label
	textWidth = font->getStrWidth(mMinXLabel);
	textPoint = Point2I(contentRect.point.x + 1, contentRect.point.y + contentRect.extent.y + 2);
	dglDrawText(font, textPoint, mMinXLabel, NULL, 0, 0);

	//x max label
	textWidth = font->getStrWidth(mMaxXLabel);
	textPoint = Point2I((contentRect.point.x + contentRect.extent.x - 1) - textWidth, contentRect.point.y + contentRect.extent.y + 2);
	dglDrawText(font, textPoint, mMaxXLabel, NULL, 0, 0);

	//y label
	textWidth = font->getStrWidth(mLabelY);
	textPoint = Point2I(contentRect.point.x - (fontHeight + 2), contentRect.point.y + (contentRect.extent.y / 2) + (textWidth / 2));
	dglDrawText(font, textPoint, mLabelY, NULL, 0, 90);

	//y min label
	textWidth = font->getStrWidth(mMinYLabel);
	textPoint = Point2I(contentRect.point.x - (textWidth + 2), (contentRect.point.y + contentRect.extent.y - 2) - (fontHeight / 2));
	dglDrawText(font, textPoint, mMinYLabel, NULL, 0, 0);

	//y max label
	textWidth = font->getStrWidth(mMaxYLabel);
	textPoint = Point2I(contentRect.point.x - (textWidth + 2), (contentRect.point.y + 4) - (fontHeight / 2));
	dglDrawText(font, textPoint, mMaxYLabel, NULL, 0, 0);
}
	
void GuiParticleGraphInspector::renderGrid(const RectI &contentRect, const ColorI &gridColor)
{
	S32 x, y;
	x = contentRect.len_x() / 10;
	y = contentRect.len_y() / 10;

	//horizontal lines
	for (U8 i = 0; i < 11; i++)
	{
		if(i != 5)
		{
			dglDrawLine(Point2I(contentRect.point.x, contentRect.point.y + (y * i)), Point2I(contentRect.point.x + contentRect.extent.x, contentRect.point.y + (y * i)), gridColor);
		}
		else
		{
			Point2I p1 = Point2I(contentRect.point.x, contentRect.point.y + (y * i) - 1);
			Point2I p2 = Point2I(contentRect.point.x + contentRect.extent.x, contentRect.point.y + (y * i) - 1);
			Point2I p3 = Point2I(contentRect.point.x + contentRect.extent.x, contentRect.point.y + (y * i) + 1);
			Point2I p4 = Point2I(contentRect.point.x, contentRect.point.y + (y * i) + 1);
			dglDrawQuadFill(p1, p2, p3, p4, gridColor);
		}
	}

	//vertical lines
	for (U8 i = 0; i < 11; i++)
	{
		if (i != 5)
		{
			dglDrawLine(Point2I(contentRect.point.x + (x * i), contentRect.point.y), Point2I(contentRect.point.x + (x * i), contentRect.point.y + contentRect.extent.y), gridColor);
		}
		else
		{
			Point2I p1 = Point2I(contentRect.point.x + (x * i) - 1, contentRect.point.y);
			Point2I p2 = Point2I(contentRect.point.x + (x * i) + 1, contentRect.point.y);
			Point2I p3 = Point2I(contentRect.point.x + (x * i) + 1, contentRect.point.y + contentRect.extent.y);
			Point2I p4 = Point2I(contentRect.point.x + (x * i) - 1, contentRect.point.y + contentRect.extent.y);
			dglDrawQuadFill(p1, p2, p3, p4, gridColor);
		}
	}
}

void GuiParticleGraphInspector::calculatePoints(const RectI &contentRect)
{
	mGridRect = RectI(contentRect);

	mPointList.clear();
	ParticleAssetField* field = getTargetField();
	
	F32 time = 0;
	U32 count = field->getDataKeyCount();
	for (U32 i = 0; i < count; i++)
	{
		ParticleAssetField::DataKey key = field->getDataKey(i);

		//force the first key to always be at time zero
		if (i == 0 && key.mTime != 0)
		{
			field->addDataKey(0, key.mValue);
			field->removeDataKey(1);
			key = field->getDataKey(0);
			count = field->getDataKeyCount();
		}

		//Remove the point if it has a bad time
		if (i > 0 && key.mTime <= time)
		{
			field->removeDataKey(i);
			count--;
			continue;
		}
		time = key.mTime;
	}

	Point2I p;
	for (U32 i = 0; i < count; i++)
	{
		ParticleAssetField::DataKey key = field->getDataKey(i);
		p = convertToRenderPoint(contentRect, key.mTime, key.mValue);
		mPointList.push_back(GraphPoint(p, key.mTime, key.mValue, i));
	}
	mDirty = false;
}

Point2I GuiParticleGraphInspector::convertToRenderPoint(const RectI& contentRect, F32 time, F32 value)
{
	F32 width = mMaxX - mMinX;
	F32 height = mMaxY - mMinY;

	F32 ratioX = (time - mMinX) / width;
	F32 ratioY = (value - mMinY) / height;

	return Point2I(contentRect.point.x + (contentRect.extent.x * ratioX), contentRect.point.y + (contentRect.extent.y * (1 - ratioY)));
}

void GuiParticleGraphInspector::renderPoints(const RectI &contentRect, const ColorI &lineColor)
{
	if (mTargetAsset)
	{
		if (mDirty)
		{
			calculatePoints(contentRect);
		}

		//get the cursor position
		Point2I cursorPt = Point2I(0, 0);
		GuiCanvas *root = getRoot();
		if (root)
		{
			cursorPt = root->getCursorPos();
		}

		//Render variation
		if (mVariationInspector != NULL)
		{
			ColorI variColor = ColorI(lineColor);
			variColor.alpha /= 2;
			renderVariation(contentRect, variColor);
		}

		//Render the lines
		Point2I p1, p2;
		U32 count = mPointList.size();
		for(U32 i = 1; i < count; i++)
		{
			p1 = mPointList[i-1].mPoint;
			p2 = mPointList[i].mPoint;

			renderLine(contentRect, p1, p2, lineColor);
			renderDot(contentRect, p1, cursorPt, mSelectedIndex == (i - 1));
		}
		p1 = mPointList[count - 1].mPoint;
		p2 = Point2I(contentRect.point.x + contentRect.extent.x, p1.y);
		if (p1.x < p2.x)
		{
			renderLine(contentRect, p1, p2, lineColor);
		}
		renderDot(contentRect, p1, cursorPt, mSelectedIndex == (count - 1));
	}
}

Vector<GuiParticleGraphInspector::GraphPoint>* GuiParticleGraphInspector::getRenderPoints()
{
	if (!mAwake)
	{
		return NULL;
	}

	if (mDirty) 
	{ 
		RectI rect = RectI(0, 0, 1, 1); 
		calculatePoints(rect); mDirty = true; 
	} 
	return &mPointList;
}

void GuiParticleGraphInspector::renderVariation(const RectI& contentRect, const ColorI& color)
{
	Vector<GraphPoint>* variPointList = mVariationInspector->getRenderPoints();
	if (!variPointList)
	{
		return;
	}

	S32 vPen = 0;
	S32 bPen = 0;
	Point2I up1, down1, up2, down2;
	while (vPen < variPointList->size() || bPen < mPointList.size())
	{
		GraphPoint& vari = variPointList->at(getMin(vPen, variPointList->size() - 1));
		GraphPoint& base = mPointList.at(getMin(bPen, mPointList.size() - 1));

		if (vPen == 0 && bPen == 0)
		{
			up1 = convertToRenderPoint(contentRect, 0, base.mValue + vari.mValue);
			down1 = convertToRenderPoint(contentRect, 0, base.mValue - vari.mValue);
			vPen = 1;
			bPen = 1;
			continue;
		}

		if (vPen >= variPointList->size() || bPen >= mPointList.size())
		{
			F32 time = vPen >= variPointList->size() ? base.mTime : vari.mTime;
			up2 = convertToRenderPoint(contentRect, time, base.mValue + vari.mValue);
			down2 = convertToRenderPoint(contentRect, time, base.mValue - vari.mValue);
			vPen++;
			bPen++;
		}
		else if (vari.mTime == base.mTime)
		{
			up2 = convertToRenderPoint(contentRect, base.mTime, base.mValue + vari.mValue);
			down2 = convertToRenderPoint(contentRect, base.mTime, base.mValue - vari.mValue);
			vPen++;
			bPen++;
		}
		else if (vari.mTime < base.mTime)
		{
			GraphPoint& oldBase = mPointList.at(bPen - 1);
			F32 timeDeltaB = base.mTime - oldBase.mTime;
			F32 timeDeltaV = vari.mTime - oldBase.mTime;
			F32 ratio = timeDeltaV / timeDeltaB;
			F32 baseValue = oldBase.mValue + ((base.mValue - oldBase.mValue) * ratio);
			up2 = convertToRenderPoint(contentRect, vari.mTime, baseValue + vari.mValue);
			down2 = convertToRenderPoint(contentRect, vari.mTime, baseValue - vari.mValue);
			vPen++;
		}
		else if (vari.mTime > base.mTime)
		{
			GraphPoint& oldVari = variPointList->at(vPen - 1);
			F32 timeDeltaB = base.mTime - oldVari.mTime;
			F32 timeDeltaV = vari.mTime - oldVari.mTime;
			F32 ratio = timeDeltaB / timeDeltaV;
			F32 variValue = oldVari.mValue + ((vari.mValue - oldVari.mValue) * ratio);
			up2 = convertToRenderPoint(contentRect, base.mTime, base.mValue + variValue);
			down2 = convertToRenderPoint(contentRect, base.mTime, base.mValue - variValue);
			bPen++;
		}
		renderQuad(contentRect, up1, up2, down1, down2, color);
		up1 = up2;
		down1 = down2;
	}
	up2 = Point2I(contentRect.point.x + contentRect.extent.x, up1.y);
	down2 = Point2I(contentRect.point.x + contentRect.extent.x, down1.y);
	if (up1.x < up2.x)
	{
		renderQuad(contentRect, up1, up2, down1, down2, color);
	}
}

void GuiParticleGraphInspector::renderDot(const RectI &contentRect, const Point2I &point, const Point2I &cursorPt, bool isSelected)
{
	if(point.x >= contentRect.point.x && point.x <= contentRect.point.x + contentRect.extent.x && point.y >= contentRect.point.y && point.y <= contentRect.point.y + contentRect.extent.y)
	{
		F32 x = cursorPt.x - point.x;
		F32 y = cursorPt.y - point.y;
		F32 dist = mSqrt((x * x) + (y * y));
		ColorI color;
		if (isSelected)
		{
			color = mProfile->getFontColor(SelectedState);
		}
		else if (dist <= mRadius)
		{
			color = mProfile->getFontColor(HighlightState);
		} 
		else
		{
			color = mProfile->getFontColor(NormalState);
		}

		dglDrawCircleFill(point, mRadius, ColorI(0, 0, 0, 100));
		dglDrawCircleFill(point, mRadius - 2, color);
	}
}

void GuiParticleGraphInspector::renderLine(const RectI &contentRect, const Point2I &point1, const Point2I &point2, const ColorI &lineColor)
{
	RectClipper clipper = RectClipper(contentRect);

	Point2I p1;
	Point2I p2;
	if(clipper.clipLine(point1, point2, p1, p2))
	{
		dglDrawLine(p1, p2, ColorI(lineColor));
	}
}

//Points are leftTop, rightTop, leftBottom, rightBottom
void GuiParticleGraphInspector::renderQuad(const RectI& contentRect, const Point2I& point1, const Point2I& point2, const Point2I& point3, const Point2I& point4, const ColorI& quadColor)
{
	//if the heights of the left and right sides are both zero then we can exit now
	if ((point1.y - point3.y) == 0 && (point2.y - point4.y) == 0)
	{
		return;
	}

	RectI area = RectI(point1.x, getMin(point1.y, point2.y), point2.x - point1.x, point1.y < point2.y ? getMax(point3.y, point4.y) - point1.y : getMax(point3.y, point4.y) - point2.y);
	if (!contentRect.overlaps(area))
	{
		//Nothing to draw here...
		return;
	}

	if ((point1.y > point4.y || point2.y > point3.y) && area.extent.y > 1)
	{
		Point2I point5 = Point2I(mRound((point1.x + point2.x) / 2), mRound((point1.y + point2.y) / 2));
		Point2I point6 = Point2I(mRound((point3.x + point4.x) / 2), mRound((point3.y + point4.y) / 2));
		renderQuad(contentRect, point1, point5, point3, point6, quadColor);
		renderQuad(contentRect, point5, point2, point6, point4, quadColor);
		return;
	}

	RectClipper clipper = RectClipper(contentRect);

	Point2I topStart;
	Point2I topEnd;
	bool hasTop = clipper.clipLine(point1, point2, topStart, topEnd);

	Point2I bottomStart;
	Point2I bottomEnd;
	bool hasBottom = clipper.clipLine(point3, point4, bottomStart, bottomEnd);

	Point2I leftStart;
	Point2I leftEnd;
	bool hasLeft = clipper.clipLine(point1, point3, leftStart, leftEnd);

	Point2I rightStart;
	Point2I rightEnd;
	bool hasRight = clipper.clipLine(point2, point4, rightStart, rightEnd);

	//Replace left and right if they're missing
	if (!hasLeft)
	{
		leftStart.x = leftEnd.x = contentRect.point.x;
		leftStart.y = hasTop ? topStart.y : contentRect.point.y;
		leftEnd.y = hasBottom ? bottomStart.y : contentRect.point.y + contentRect.extent.y;
	}
	if (!hasRight)
	{
		rightStart.x = rightEnd.x = contentRect.point.x + contentRect.extent.x - 1;
		rightStart.y = hasTop ? topEnd.y : contentRect.point.y;
		rightEnd.y = hasBottom ? bottomEnd.y : contentRect.point.y + contentRect.extent.y;
	}

	S32 leftEdge = leftStart.x;
	S32 rightEdge = rightStart.x;

	//Middle Section
	S32 y = getMax(leftStart.y, rightStart.y);
	S32 h = getMin(leftEnd.y - y, rightEnd.y - y);
	RectI fillRect = RectI(leftStart.x, y, rightStart.x - leftStart.x, h);
	dglDrawRectFill(fillRect, quadColor);

	//Top Section
	if (hasTop && topStart.y != topEnd.y)
	{
		if (leftEdge != topStart.x && topStart.y < topEnd.y)
		{
			RectI rect = RectI(leftEdge, topStart.y, topStart.x - leftEdge, topEnd.y - topStart.y);
			dglDrawRectFill(rect, quadColor);

			Point2I p = Point2I(topStart.x, topEnd.y);
			dglDrawTriangleFill(topStart, p, topEnd, quadColor);
		}
		else if (rightEdge != topEnd.x && topStart.y > topEnd.y)
		{
			RectI rect = RectI(topEnd.x, topEnd.y, rightEdge - topEnd.x, topStart.y - topEnd.y);
			dglDrawRectFill(rect, quadColor);

			Point2I p = Point2I(topEnd.x, topStart.y);
			dglDrawTriangleFill(topStart, p, topEnd, quadColor);
		}
		else if (topStart.y > topEnd.y)
		{
			Point2I p = Point2I(topEnd.x, topStart.y);
			dglDrawTriangleFill(topStart, p, topEnd, quadColor);
		}
		else if (topStart.y < topEnd.y)
		{
			Point2I p = Point2I(topStart.x, topEnd.y);
			dglDrawTriangleFill(topStart, p, topEnd, quadColor);
		}
	}

	//Bottom Section
	if (hasBottom && bottomStart.y != bottomEnd.y)
	{
		if (leftEdge != bottomStart.x && bottomStart.y > bottomEnd.y)
		{
			RectI rect = RectI(leftEdge, bottomEnd.y, bottomStart.x - leftEdge, bottomStart.y - bottomEnd.y);
			dglDrawRectFill(rect, quadColor);

			Point2I p = Point2I(bottomStart.x, bottomEnd.y);
			dglDrawTriangleFill(bottomEnd, p, bottomStart, quadColor);
		}
		else if (rightEdge != bottomEnd.x && bottomStart.y < bottomEnd.y)
		{
			RectI rect = RectI(bottomEnd.x, bottomStart.y, rightEdge - bottomEnd.x, bottomEnd.y - bottomStart.y);
			dglDrawRectFill(rect, quadColor);

			Point2I p = Point2I(bottomEnd.x, bottomStart.y);
			dglDrawTriangleFill(bottomStart, bottomEnd, p, quadColor);
		}
		else if (bottomStart.y < bottomEnd.y)
		{
			Point2I p = Point2I(bottomEnd.x, bottomStart.y);
			dglDrawTriangleFill(bottomStart, bottomEnd, p, quadColor);
		}
		else if (bottomStart.y > bottomEnd.y)
		{
			Point2I p = Point2I(bottomStart.x, bottomEnd.y);
			dglDrawTriangleFill(bottomStart, bottomEnd, p, quadColor);
		}
	}
}