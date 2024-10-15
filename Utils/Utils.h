#ifndef UTILS_H
#define UTILS_H
#include <QDir>

struct Utils
{
    static bool mkDirs(QString dirPath)
    {
        QDir dir(dirPath);
        if(dir.exists())
        {
            return true;
        }
        else
        {
            bool result = dir.mkpath(dirPath);
            return result;
        }
    }
};

#endif // UTILS_H
