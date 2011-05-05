/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the FOO module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QAbstractFileEngine>
#include <QtCore/QFSFileEngine>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QSharedPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QHash>

#include <QtTest/QTest>

#include <QtCore/QDebug>

class tst_QAbstractFileEngine
    : public QObject
{
    Q_OBJECT
public slots:
    void cleanupTestCase();

private slots:
    void customHandler();

    void fileIO_data();
    void fileIO();

private:
    QStringList filesForRemoval;
};

class ReferenceFileEngine
    : public QAbstractFileEngine
{
public:
    ReferenceFileEngine(const QString &fileName)
        : fileName_(fileName)
        , position_(-1)
        , openForRead_(false)
        , openForWrite_(false)
    {
    }

    bool open(QIODevice::OpenMode openMode)
    {
        if (openForRead_ || openForWrite_) {
            qWarning("%s: file is already open for %s",
                     Q_FUNC_INFO,
                     (openForRead_ ? "reading" : "writing"));
            return false;
        }

        openFile_ = resolveFile(openMode & QIODevice::WriteOnly);
        if (!openFile_)
            return false;

        position_ = 0;
        if (openMode & QIODevice::ReadOnly)
            openForRead_ = true;

        if (openMode & QIODevice::WriteOnly) {
            openForWrite_ = true;

            QMutexLocker lock(&openFile_->mutex);
            if (openMode & QIODevice::Truncate
                    || !(openForRead_ || openMode & QIODevice::Append))
                openFile_->content.clear();

            if (openMode & QIODevice::Append)
                position_ = openFile_->content.size();
        }

        return true;
    }

    bool close()
    {
        openFile_.clear();

        openForRead_ = false;
        openForWrite_ = false;
        position_ = -1;

        return true;
    }

    qint64 size() const
    {
        QSharedPointer<File> file = resolveFile(false);
        if (!file)
            return 0;

        QMutexLocker lock(&file->mutex);
        return file->content.size();
    }

    qint64 pos() const
    {
        if (!openForRead_ && !openForWrite_) {
            qWarning("%s: file is not open", Q_FUNC_INFO);
            return -1;
        }
        return position_;
    }

    bool seek(qint64 pos)
    {
        if (!openForRead_ && !openForWrite_) {
            qWarning("%s: file is not open", Q_FUNC_INFO);
            return false;
        }

        if (pos >= 0) {
            position_ = pos;
            return true;
        }

        return false;
    }

    bool flush()
    {
        if (!openForRead_ && !openForWrite_) {
            qWarning("%s: file is not open", Q_FUNC_INFO);
            return false;
        }

        return true;
    }

    bool remove()
    {
        QMutexLocker lock(&fileSystemMutex);
        int count = fileSystem.remove(fileName_);

        return (count == 1);
    }

    bool copy(const QString &newName)
    {
        QMutexLocker lock(&fileSystemMutex);
        if (!fileSystem.contains(fileName_)
                || fileSystem.contains(newName))
            return false;

        fileSystem.insert(newName, fileSystem.value(fileName_));
        return true;
    }

    bool rename(const QString &newName)
    {
        QMutexLocker lock(&fileSystemMutex);
        if (!fileSystem.contains(fileName_)
                || fileSystem.contains(newName))
            return false;

        fileSystem.insert(newName, fileSystem.take(fileName_));
        return true;
    }

    //  bool link(const QString &newName)
    //  {
    //      Q_UNUSED(newName)
    //      return false;
    //  }

    //  bool mkdir(const QString &dirName, bool createParentDirectories) const
    //  {
    //      Q_UNUSED(dirName)
    //      Q_UNUSED(createParentDirectories)

    //      return false;
    //  }

    //  bool rmdir(const QString &dirName, bool recurseParentDirectories) const
    //  {
    //      Q_UNUSED(dirName)
    //      Q_UNUSED(recurseParentDirectories)

    //      return false;
    //  }

    bool setSize(qint64 size)
    {
        if (size < 0)
            return false;

        QSharedPointer<File> file = resolveFile(false);
        if (!file)
            return false;

        QMutexLocker lock(&file->mutex);
        file->content.resize(size);

        if (openForRead_ || openForWrite_)
            if (position_ > size)
                position_ = size;

        return (file->content.size() == size);
    }

    FileFlags fileFlags(FileFlags type) const
    {
        QSharedPointer<File> file = resolveFile(false);
        if (file) {
            QMutexLocker lock(&file->mutex);
            return (file->fileFlags & type);
        }

        return FileFlags();
    }

    //  bool setPermissions(uint perms)
    //  {
    //      Q_UNUSED(perms)

    //      return false;
    //  }

    QString fileName(FileName file) const
    {
        switch (file) {
            case DefaultName:
                return QLatin1String("DefaultName");
            case BaseName:
                return QLatin1String("BaseName");
            case PathName:
                return QLatin1String("PathName");
            case AbsoluteName:
                return QLatin1String("AbsoluteName");
            case AbsolutePathName:
                return QLatin1String("AbsolutePathName");
            case LinkName:
                return QLatin1String("LinkName");
            case CanonicalName:
                return QLatin1String("CanonicalName");
            case CanonicalPathName:
                return QLatin1String("CanonicalPathName");
            case BundleName:
                return QLatin1String("BundleName");

            default:
                break;
        }

        return QString();
    }

    uint ownerId(FileOwner owner) const
    {
        QSharedPointer<File> file = resolveFile(false);
        if (file) {
            switch (owner) {
                case OwnerUser:
                {
                    QMutexLocker lock(&file->mutex);
                    return file->userId;
                }
                case OwnerGroup:
                {
                    QMutexLocker lock(&file->mutex);
                    return file->groupId;
                }
            }
        }

        return -2;
    }

    QString owner(FileOwner owner) const
    {
        QSharedPointer<File> file = resolveFile(false);
        if (file) {
            uint ownerId;
            switch (owner) {
                case OwnerUser:
                {
                    QMutexLocker lock(&file->mutex);
                    ownerId = file->userId;
                }

                {
                    QMutexLocker lock(&fileSystemMutex);
                    return fileSystemUsers.value(ownerId);
                }

                case OwnerGroup:
                {
                    QMutexLocker lock(&file->mutex);
                    ownerId = file->groupId;
                }

                {
                    QMutexLocker lock(&fileSystemMutex);
                    return fileSystemGroups.value(ownerId);
                }
            }
        }

        return QString();
    }

    QDateTime fileTime(FileTime time) const
    {
        QSharedPointer<File> file = resolveFile(false);
        if (file) {
            QMutexLocker lock(&file->mutex);
            switch (time) {
                case CreationTime:
                    return file->creation;
                case ModificationTime:
                    return file->modification;
                case AccessTime:
                    return file->access;
            }
        }

        return QDateTime();
    }

    void setFileName(const QString &file)
    {
        Q_ASSERT(!openForRead_);
        Q_ASSERT(!openForWrite_);

        fileName_ = file;
    }

    //  typedef QAbstractFileEngineIterator Iterator;
    //  Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames)
    //  {
    //      Q_UNUSED(filters)
    //      Q_UNUSED(filterNames)

    //      return 0;
    //  }

    //  Iterator *endEntryList()
    //  {
    //      return 0;
    //  }

    qint64 read(char *data, qint64 maxLen)
    {
        if (!openForRead_) {
            qWarning("%s: file must be open for reading", Q_FUNC_INFO);
            return -1;
        }

        if (openFile_.isNull()) {
            qWarning("%s: file must not be null", Q_FUNC_INFO);
            return -1;
        }

        QMutexLocker lock(&openFile_->mutex);
        qint64 readSize = qMin(openFile_->content.size() - position_, maxLen);
        if (readSize < 0)
            return -1;

        qMemCopy(data, openFile_->content.constData() + position_, readSize);
        position_ += readSize;

        return readSize;
    }

    qint64 write(const char *data, qint64 length)
    {
        if (!openForWrite_) {
            qWarning("%s: file must be open for writing", Q_FUNC_INFO);
            return -1;
        }

        if (openFile_.isNull()) {
            qWarning("%s: file must not be null", Q_FUNC_INFO);
            return -1;
        }

        if (length < 0)
            return -1;

        QMutexLocker lock(&openFile_->mutex);
        if (openFile_->content.size() == position_)
            openFile_->content.append(data, length);
        else {
            if (position_ + length > openFile_->content.size())
                openFile_->content.resize(position_ + length);
            openFile_->content.replace(position_, length, data, length);
        }

        qint64 writeSize = qMin(length, openFile_->content.size() - position_);
        position_ += writeSize;

        return writeSize;
    }

protected:
    // void setError(QFile::FileError error, const QString &str);

    struct File
    {
        File()
            : userId(0)
            , groupId(0)
            , fileFlags(
                    ReadOwnerPerm | WriteOwnerPerm | ExeOwnerPerm
                    | ReadUserPerm | WriteUserPerm | ExeUserPerm
                    | ReadGroupPerm | WriteGroupPerm | ExeGroupPerm
                    | ReadOtherPerm | WriteOtherPerm | ExeOtherPerm
                    | FileType | ExistsFlag)
        {
        }

        QMutex mutex;

        uint userId, groupId;
        QAbstractFileEngine::FileFlags fileFlags;
        QDateTime creation, modification, access;

        QByteArray content;
    };

    QSharedPointer<File> resolveFile(bool create) const
    {
        if (openForRead_ || openForWrite_) {
            if (!openFile_)
                qWarning("%s: file should not be null", Q_FUNC_INFO);
            return openFile_;
        }

        QMutexLocker lock(&fileSystemMutex);
        if (create) {
            QSharedPointer<File> &p = fileSystem[fileName_];
            if (p.isNull())
                p = QSharedPointer<File>(new File);
            return p;
        }

        return fileSystem.value(fileName_);
    }

    static QMutex fileSystemMutex;
    static QHash<uint, QString> fileSystemUsers, fileSystemGroups;
    static QHash<QString, QSharedPointer<File> > fileSystem;

private:
    QString fileName_;
    qint64 position_;
    bool openForRead_;
    bool openForWrite_;

    mutable QSharedPointer<File> openFile_;
};

QMutex ReferenceFileEngine::fileSystemMutex;
QHash<uint, QString> ReferenceFileEngine::fileSystemUsers, ReferenceFileEngine::fileSystemGroups;
QHash<QString, QSharedPointer<ReferenceFileEngine::File> > ReferenceFileEngine::fileSystem;

class FileEngineHandler
    : QAbstractFileEngineHandler
{
    QAbstractFileEngine *create(const QString &fileName) const
    {
        if (fileName.startsWith("QFSFileEngine:"))
            return new QFSFileEngine(fileName.mid(14));
        if (fileName.startsWith("reference-file-engine:"))
            return new ReferenceFileEngine(fileName.mid(22));
        if (fileName.startsWith("resource:"))
            return QAbstractFileEngine::create(QLatin1String(":/tst_qabstractfileengine/resources/") + fileName.mid(9));
        return 0;
    }
};

void tst_QAbstractFileEngine::cleanupTestCase()
{
    bool failed = false;

    FileEngineHandler handler;
    Q_FOREACH(QString file, filesForRemoval)
        if (!QFile::remove(file)
                || QFile::exists(file)) {
            failed = true;
            qDebug() << "Couldn't remove file:" << file;
        }

    QVERIFY(!failed);
}

void tst_QAbstractFileEngine::customHandler()
{
    QScopedPointer<QAbstractFileEngine> file;
    {
        file.reset(QAbstractFileEngine::create("resource:file.txt"));

        QVERIFY(file);
    }

    {
        FileEngineHandler handler;

        QFile file("resource:file.txt");
        QVERIFY(file.exists());
    }

    {
        QFile file("resource:file.txt");
        QVERIFY(!file.exists());
    }
}

void tst_QAbstractFileEngine::fileIO_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QByteArray>("readContent");
    QTest::addColumn<QByteArray>("writeContent");
    QTest::addColumn<bool>("fileExists");

    QString resourceTxtFile(":/tst_qabstractfileengine/resources/file.txt");
    QByteArray readContent("This is a simple text file.\n");
    QByteArray writeContent("This contains two lines of text.\n");

    QTest::newRow("resource") << resourceTxtFile << readContent << QByteArray() << true;
    QTest::newRow("native") << "native-file.txt" << readContent << writeContent << false;
    QTest::newRow("Forced QFSFileEngine") << "QFSFileEngine:QFSFileEngine-file.txt" << readContent << writeContent << false;
    QTest::newRow("Custom FE") << "reference-file-engine:file.txt" << readContent << writeContent << false;

    QTest::newRow("Forced QFSFileEngine (native)") << "QFSFileEngine:native-file.txt" << readContent << writeContent << true;
    QTest::newRow("native (Forced QFSFileEngine)") << "QFSFileEngine-file.txt" << readContent << writeContent << true;
    QTest::newRow("Custom FE (2)") << "reference-file-engine:file.txt" << readContent << writeContent << true;
}

void tst_QAbstractFileEngine::fileIO()
{
    QFETCH(QString, fileName);
    QFETCH(QByteArray, readContent);
    QFETCH(QByteArray, writeContent);
    QFETCH(bool, fileExists);

    FileEngineHandler handler;


    {
        QFile file(fileName);
        QCOMPARE(file.exists(), fileExists);

        if (!fileExists) {
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Unbuffered));
            filesForRemoval.append(fileName);

            QCOMPARE(file.write(readContent), qint64(readContent.size()));
        }
    }

    //
    // File content is: readContent
    //

    qint64 fileSize = readContent.size();
    {
        // Reading
        QFile file(fileName);

        QVERIFY(!file.isOpen());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Unbuffered));

        QVERIFY(file.isOpen());
        QCOMPARE(file.size(), fileSize);
        QCOMPARE(file.pos(), qint64(0));

        QCOMPARE(file.size(), fileSize);
        QCOMPARE(file.readAll(), readContent);
        QCOMPARE(file.pos(), fileSize);

        file.close();
        QVERIFY(!file.isOpen());
        QCOMPARE(file.size(), fileSize);
    }

    if (writeContent.isEmpty())
        return;

    {
        // Writing / appending
        QFile file(fileName);

        QVERIFY(!file.isOpen());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered));

        QVERIFY(file.isOpen());
        QCOMPARE(file.size(), fileSize);
        QCOMPARE(file.pos(), fileSize);

        QCOMPARE(file.write(writeContent), qint64(writeContent.size()));

        fileSize += writeContent.size();
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        file.close();
        QVERIFY(!file.isOpen());
        QCOMPARE(file.size(), fileSize);
    }

    //
    // File content is: readContent + writeContent
    //

    {
        // Reading and Writing
        QFile file(fileName);

        QVERIFY(!file.isOpen());
        QVERIFY(file.open(QIODevice::ReadWrite | QIODevice::Unbuffered));

        QVERIFY(file.isOpen());
        QCOMPARE(file.size(), fileSize);
        QCOMPARE(file.pos(), qint64(0));

        QCOMPARE(file.readAll(), readContent + writeContent);
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        QVERIFY(file.seek(writeContent.size()));
        QCOMPARE(file.pos(), qint64(writeContent.size()));
        QCOMPARE(file.size(), fileSize);

        QCOMPARE(file.write(readContent), qint64(readContent.size()));
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        QVERIFY(file.seek(0));
        QCOMPARE(file.pos(), qint64(0));
        QCOMPARE(file.size(), fileSize);

        QCOMPARE(file.write(writeContent), qint64(writeContent.size()));
        QCOMPARE(file.pos(), qint64(writeContent.size()));
        QCOMPARE(file.size(), fileSize);

        QVERIFY(file.seek(0));
        QCOMPARE(file.read(writeContent.size()), writeContent);
        QCOMPARE(file.pos(), qint64(writeContent.size()));
        QCOMPARE(file.size(), fileSize);

        QCOMPARE(file.readAll(), readContent);
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        file.close();
        QVERIFY(!file.isOpen());
        QCOMPARE(file.size(), fileSize);
    }

    //
    // File content is: writeContent + readContent
    //

    {
        // Writing
        QFile file(fileName);

        QVERIFY(!file.isOpen());
        QVERIFY(file.open(QIODevice::ReadWrite | QIODevice::Unbuffered));

        QVERIFY(file.isOpen());
        QCOMPARE(file.size(), fileSize);
        QCOMPARE(file.pos(), qint64(0));

        QCOMPARE(file.write(writeContent), qint64(writeContent.size()));
        QCOMPARE(file.pos(), qint64(writeContent.size()));
        QCOMPARE(file.size(), fileSize);

        QVERIFY(file.resize(writeContent.size()));
        QCOMPARE(file.size(), qint64(writeContent.size()));

        file.close();
        QVERIFY(!file.isOpen());
        QCOMPARE(file.size(), qint64(writeContent.size()));

        QVERIFY(file.resize(fileSize));
        QCOMPARE(file.size(), fileSize);
    }

    //
    // File content is: writeContent + <undefined>
    // File size is   : (readContent + writeContent).size()
    //

    {
        // Writing / extending
        QFile file(fileName);

        QVERIFY(!file.isOpen());
        QVERIFY(file.open(QIODevice::ReadWrite | QIODevice::Unbuffered));

        QVERIFY(file.isOpen());
        QCOMPARE(file.size(), fileSize);
        QCOMPARE(file.pos(), qint64(0));

        QVERIFY(file.seek(1024));
        QCOMPARE(file.pos(), qint64(1024));
        QCOMPARE(file.size(), fileSize);

        fileSize = 1024 + writeContent.size();
        QCOMPARE(file.write(writeContent), qint64(writeContent.size()));
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        QVERIFY(file.seek(1028));
        QCOMPARE(file.pos(), qint64(1028));
        QCOMPARE(file.size(), fileSize);

        fileSize = 1028 + writeContent.size();
        QCOMPARE(file.write(writeContent), qint64(writeContent.size()));
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        file.close();
        QVERIFY(!file.isOpen());
        QCOMPARE(file.size(), fileSize);
    }

    //
    // File content is: writeContent + <undefined> + writeContent
    // File size is   : 1024 + writeContent.size()
    //

    {
        // Writing / truncating
        QFile file(fileName);

        QVERIFY(!file.isOpen());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Unbuffered));

        QVERIFY(file.isOpen());
        QCOMPARE(file.size(), qint64(0));
        QCOMPARE(file.pos(), qint64(0));

        fileSize = readContent.size();
        QCOMPARE(file.write(readContent), fileSize);
        QCOMPARE(file.pos(), fileSize);
        QCOMPARE(file.size(), fileSize);

        file.close();
        QVERIFY(!file.isOpen());
        QCOMPARE(file.size(), fileSize);
    }

    //
    // File content is: readContent
    //
}

QTEST_APPLESS_MAIN(tst_QAbstractFileEngine)
#include "tst_qabstractfileengine.moc"

