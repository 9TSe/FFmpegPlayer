#include "widget.h"
#include <QsLog.h>
#include <QApplication>
#include "Utils.h"

void initLogger(const QString &dir)
{
    if(Utils::mkDirs(dir))
    {
        QsLogging::Logger& logger = QsLogging::Logger::instance();
        logger.setLoggingLevel(QsLogging::TraceLevel); //最详细级别
        QString logFile = QString("%1/run.log").arg(dir);
        // @param :
        //          logfile: 日志写入该文件
        //          enablelogrotation: 日志轮换(日志文件达到一定大小自动生成新的文件)
        //          maxsizebytes: 当日志达到这个大小启动日志轮换
        //          maxoldlogcount: 最大日志文件数量, 即旧日志会被新日志替代
        QsLogging::DestinationPtr desPtr(QsLogging::DestinationFactory::MakeFileDestination
                                    (logFile, QsLogging::EnableLogRotation,
                                    QsLogging::MaxSizeBytes(1*1024*1024),QsLogging::MaxOldLogCount(1)));
        logger.addDestination(desPtr);
        QLOG_INFO() <<  "initLogger() sucess, logDir = " << dir;
    }
    else{
        QLOG_INFO() << "initLogger() fail, logDir = " << dir;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName("FFmpegPlayer");
    QCoreApplication::setApplicationVersion("0.0.2");

#if(QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
#elif(QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication a(argc, argv);

    QFile qssFile(":/res/qss/widget.qss");
    if(qssFile.open(qssFile.ReadOnly)){
        a.setStyleSheet(qssFile.readAll());
    }

    const QString logDir = QApplication::applicationDirPath() + "/logs";
    initLogger(logDir);

    Widget w;
    w.show();
    return a.exec();
}
