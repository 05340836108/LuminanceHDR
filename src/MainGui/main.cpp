/**
 * This file is a part of Luminance HDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2006,2007 Giuseppe Rota
 * Copyright (C) 2012 Davide Anastasia
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * @author Giuseppe Rota <grota@users.sourceforge.net>
 * @author Davide Anastasia <davideanastasia@users.sourceforge.net>
 */

#include <QApplication>
#include <QObject>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QStringList>

#include "Common/global.h"
#include "Common/config.h"
#include "Common/TranslatorManager.h"
#include "MainWindow/MainWindow.h"

#ifdef WIN32
#include <lcms2.h>

#endif

namespace
{
QStringList getCliFiles(const QStringList& arguments)
{
    // empty QStringList;
    QStringList fileList;

    // check if any of the parameters is a proper file on the file system
    // I skip the first value of the list because it is the name of the executable
    for (int i = 1; i < arguments.size(); ++i) {
        QFile file( arguments.at(i).toLocal8Bit() );

        if ( file.exists() ) {
            fileList.push_back( arguments.at(i).toLocal8Bit() );
        }
    }

    return fileList;
}
}

#ifdef WIN32
void customMessageHandler(QtMsgType type, const char *msg)
{
	QString txt;
	switch (type) {
	case QtDebugMsg:
		txt = QString("Debug: %1").arg(msg);
		break;
	case QtWarningMsg:
		txt = QString("Warning: %1").arg(msg);
	break;
	case QtCriticalMsg:
		txt = QString("Critical: %1").arg(msg);
	break;
	case QtFatalMsg:
		txt = QString("Fatal: %1").arg(msg);
		abort();
	}

	QFile outFile("debuglog.txt");
	if (outFile.open(QIODevice::WriteOnly | QIODevice::Append))
	{
        QTextStream ts(&outFile);
        ts << txt << endl;
	}
}

void LcmsErrorHandler(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *Text)
{
    QString txt = QString("[LCMS] Error %1: %2").arg(ErrorCode).arg(Text);
    QFile outFile("errorlog.txt");
	if (outFile.open(QIODevice::WriteOnly | QIODevice::Append))
	{
        QTextStream ts(&outFile);
        ts << txt << endl;
	}
}
#endif

int main( int argc, char ** argv )
{
    Q_INIT_RESOURCE(icons);
    QApplication application( argc, argv );

#ifdef WIN32
    qInstallMsgHandler(customMessageHandler);
    cmsSetLogErrorHandler(LcmsErrorHandler);
#endif

    QCoreApplication::setApplicationName(LUMINANCEAPPLICATION);
    QCoreApplication::setOrganizationName(LUMINANCEORGANIZATION);

    LuminanceOptions::isCurrentPortableMode =
            QDir(QApplication::applicationDirPath()).exists("PortableMode.txt");

    if (LuminanceOptions::isCurrentPortableMode)
    {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,
                           QSettings::UserScope, QDir::currentPath());
    }

    LuminanceOptions::conditionallyDoUpgrade();
    TranslatorManager::setLanguage( LuminanceOptions().getGuiLang() );

    MainWindow* mainWindow = new MainWindow;

    mainWindow->show();
    mainWindow->openFiles( getCliFiles( application.arguments() ) );

    return application.exec();
}

