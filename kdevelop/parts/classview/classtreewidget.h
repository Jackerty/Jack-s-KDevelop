/***************************************************************************
 *   Copyright (C) 1999 by Jonas Nordin                                    *
 *   jonas.nordin@syncom.se                                                *
 *   Copyright (C) 2000 by Bernd Gehrmann                                  *
 *   bernd@kdevelop.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _CLASSTREEWIDGET_H_
#define _CLASSTREEWIDGET_H_

#include "classtreebase.h"


class ClassTreeWidget : public ClassTreeBase
{
    Q_OBJECT
    
public:
    ClassTreeWidget(ClassView *part);
    ~ClassTreeWidget();

protected:
    virtual KPopupMenu *createPopup();

private slots:
    void setLanguageSupport(KDevLanguageSupport *ls);
    void refresh();
    void slotTreeModeChanged();
    void slotScopeModeChanged();

private:
    void buildTree(bool fromScratch);
    void buildTreeByCategory(bool fromScratch);
    void buildTreeByNamespace(bool fromScratch);
};

#endif
