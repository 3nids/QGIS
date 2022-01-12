/***************************************************************************
    qgsmaptoolshapeellipseabstract.cpp  -  map tool for adding ellipse
    ---------------------
    begin                : July 2017
    copyright            : (C) 2017
    email                : lbartoletti at tuxfamily dot org
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmaptoolshapeellipseabstract.h"
#include "qgsgeometryrubberband.h"
#include "qgsgeometryutils.h"
#include "qgslinestring.h"
#include "qgsmapcanvas.h"
#include "qgspoint.h"
#include "qgisapp.h"
#include "qgssnapindicator.h"


void QgsMapToolShapeEllipseAbstract::deactivate()
{
  if ( !mParentTool || mEllipse.isEmpty() )
  {
    return;
  }

  mParentTool->clearCurve();

  std::unique_ptr<QgsLineString> ls( mEllipse.toLineString( segments() ) );

  mParentTool->addCurve( ls.release() );
  clean();

  QgsMapToolCapture::deactivate();
}

void QgsMapToolShapeEllipseAbstract::clean()
{
  QgsMapToolAddAbstract::clean();
  mEllipse = QgsEllipse();
}
