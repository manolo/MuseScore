/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2022 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "loadingscreenview.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QFontDatabase>
#include <QScreen>
#include <QSvgRenderer>

#include "translation.h"

using namespace mu::appshell;

static const QString imagePath(":/resources/LoadingScreen.svg");

static constexpr QSize loadingScreenSize(800, 380);

static const QColor messageColor("#F1F1EE");

static const QString website("github.com/manolo/MuseScore");
static constexpr QRectF websiteRect(loadingScreenSize.width() - 48, loadingScreenSize.height() - 48, 0, 0);

static const QColor versionNumberColor("#E8845C");
static constexpr qreal versionNumberSpacing = 5.0;

LoadingScreenView::LoadingScreenView(QWidget* parent)
    : QWidget(parent), muse::Contextable(muse::iocCtxForQWidget(this)),
    m_backgroundRenderer(new QSvgRenderer(imagePath, this))
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(loadingScreenSize);
}

bool LoadingScreenView::event(QEvent* event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(this);
        painter.setLayoutDirection(layoutDirection());
        draw(&painter);
    }

    return QWidget::event(event);
}

void LoadingScreenView::draw(QPainter* painter)
{
    painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

    // Draw background
    m_backgroundRenderer->render(painter);

    // Draw the fork's name, and what it is based on. The artwork leaves this
    // area empty on purpose; the attribution belongs where people can see it.
    {
        QFont nameFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        nameFont.setPixelSize(44);
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(QPen(QColor("#F3E7E1")));
        painter->drawText(QRectF(330, 150, 430, 56),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("PlectroScore"));

        // Their wordmark, not ours, and smaller than ours: this says what the
        // build is based on. Kept as an image because it is a mark, not type.
        QFont basedFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        basedFont.setPixelSize(16);
        painter->setFont(basedFont);
        painter->setPen(QPen(QColor("#C79A87")));
        painter->drawText(QRectF(332, 208, 200, 22),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("based on"));

        QPixmap wordmark(QStringLiteral(":/resources/musescore-wordmark.png"));
        if (!wordmark.isNull()) {
            const int wordmarkWidth = 208;
            const int wordmarkHeight = wordmark.height() * wordmarkWidth / wordmark.width();
            painter->drawPixmap(QRect(412, 208 + (22 - wordmarkHeight) / 2,
                                      wordmarkWidth, wordmarkHeight),
                                wordmark);
        }
    }

    // Draw message
    QFont font(QString::fromStdString(uiConfiguration()->fontFamily()));
    font.setPixelSize(uiConfiguration()->fontSize());

    painter->setFont(font);

    QPen pen(messageColor);
    painter->setPen(pen);

    Qt::AlignmentFlag alignment = layoutDirection() == Qt::RightToLeft ? Qt::AlignLeft : Qt::AlignRight;

    // Draw website URL
    QRectF websiteBoundingRect;
    painter->drawText(websiteRect, Qt::AlignBottom | alignment | Qt::TextDontClip, website, &websiteBoundingRect);

    // Draw version number
    pen.setColor(versionNumberColor);
    painter->setPen(pen);

    painter->drawText(websiteRect.translated(0.0, -websiteBoundingRect.height() - versionNumberSpacing),
                      Qt::AlignBottom | alignment | Qt::TextDontClip,
                      muse::qtrc("appshell", "Version %1").arg(application()->fullVersion().toString()));
}
