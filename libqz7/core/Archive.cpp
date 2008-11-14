#include "qz7/Archive.h"
#include "qz7/Stream.h"
#include "qz7/Volume.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace qz7 {

Archive::Archive(Volume *parent)
    : QObject(parent), mVolume(parent), mAddedItems(0), mNextId(0)
{
}

Archive::~Archive()
{
}

bool Archive::extractTo(uint id, QIODevice *target)
{
    QioWriteStream ws(target);
    return extractTo(id, &ws);
}

bool Archive::writeTo(QIODevice *target)
{
    QioWriteStream ws(target);
    return writeTo(&ws);
}

void Archive::replaceItem(uint id, const ArchiveItem& item)
{
    if (id < mItems.size()) {
        mModifications.insert(id, item);
    } else {
        if (mModifications.contains(id))
            mModifications.insert(id, item);
        else
            Q_ASSERT_X(false, "Archive::replaceItem", "id is not valid");
    }
}

void Archive::deleteItem(uint id)
{
    if (id < mItems.size()) {
        mModifications.insert(id, ArchiveItem());
    } else {
        if (mModifications.contains(id)) {
            mModifications.remove(id);
            mAddedItems--;
        } else {
            Q_ASSERT_X(false, "Archive::deleteItem", "id does not exist");
        }
    }
}

uint Archive::appendItem(const ArchiveItem& item)
{
    // even if count() is zero, mNextId won't be after this appendItem() call
    if (!mNextId)
        mNextId = count();

    uint id = mNextId++;
    mAddedItems++;
    mModifications.insert(id, item);
    return id;
}

QString Archive::errorString() const
{
    return mErrorString;
}

uint Archive::count() const
{
    return mItems.size() + mAddedItems;
}

ArchiveItem Archive::item(uint id) const
{
    if (mModifications.contains(id))
        return mModifications.value(id);
    if (id < mItems.size())
        return mItems.at(id);
    Q_ASSERT_X(false, "Archive::item", "id does not exist");
}

QVariant Archive::property(const QString& prop)
{
    return mProperties.value(prop);
}

SeekableReadStream *Archive::openFile(uint n)
{
    return mVolume->openFile(n);
}

void Archive::addItem(const ArchiveItem& item)
{
    mItems.append(item);
}

void Archive::setProperty(const QString& prop, const QVariant& val)
{
    mProperties.insert(prop, val);
}

void Archive::setErrorString(const QString& str)
{
    mErrorString = str;
}

QList<ArchiveItem> Archive::normalizedItems() const
{
    typedef QMap<uint, ArchiveItem> ModificationMap;

    QList<ArchiveItem> ret;
    ModificationMap mods = mModifications;

    // go through all the preexisting items and add either them or their modification to
    // the normalized list
    for (int i = 0, size = mItems.size(); i < size; i++) {
        ModificationMap::iterator it = mods.find(i);

        if (it != mods.end()) {
            if (it.value().isValid())
                ret.append(it.value());
            mods.erase(it);
        } else {
            ret.append(mItems.at(i));
        }
    }

    // any remaining modifications are additions
    ret << mods.values();

    return ret;
}

} // namespace qz7
