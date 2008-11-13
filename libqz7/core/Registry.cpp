#include "qz7/Plugin.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QPluginLoader>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace qz7 {

Registry::Registry()
{
    loadPlugins();
}

Registry::~Registry()
{
}

void Registry::loadPlugins()
{
    // load the static plugins
    foreach (QObject *instance, QPluginLoader::staticInstances())
        registerPlugin(instance);

    QSet<QString> considered;

    // and then load dynamic plugins from $QT_PLUGIN_PATH/qz7
    foreach (const QString& libDir, QCoreApplication::libraryPaths()) {
        QDir pluginsDir(libDir);

        if (!pluginsDir.cd("qz7"))
            continue;

        foreach (const QString& fileName, pluginsDir.entryList(QDir::Files)) {
            if (fileName == "." || fileName == "..")
                continue;

            QString file = pluginsDir.absoluteFilePath(fileName);

            // don't try to load a plugin more than once
            if (considered.contains(file))
                continue;
            considered.insert(file);

            QPluginLoader loader(file);
            QObject *plugin = loader.instance();
            if (plugin) {
                registerPlugin(plugin);
            }
        }
}

void Registry::registerPlugin(QObject *instance)
{
    if (ArchiveFactory *af = qobject_cast<ArchiveFactory *>(instance)) {
        foreach (const QString& mime, af->archiveMimeTypes()) {
            archivesByMimeType.insert(mime, instance);
        }
    }

    if (CodecFactory *cf = qobject_cast<CodecFactory *>(instance)) {
        foreach (const QString& name, cf->codecNames()) {
            codecsByName.insert(name, instance);
        }
        foreach (int id, cf->codecIds()) {
            codecsById.insert(id, instance);
        }
    }

    if (VolumeFactory *vf = qobject_cast<VolumeFactory *>(instance)) {
        foreach (const QString& type, vf->volumeMimeTypes()) {
            volumesByName.insert(type, instance);
        }
    }
}

QObject *Registry::findArchive(const QString& mimeType)
{
    return archivesByMimeType.value(mimeType);
}

QObject *Registry::findCodec(const QString& name)
{
    return codecsByName.value(name);
}

QObject *Registry::findCodec(int id)
{
    return codecsById.value(id);
}

QObject *Registry::findVolume(const QString& type)
{
    return volumesByMimeType.value(type);
}

Registry *Registry::the()
{
    static Registry *s_instance = 0;

    if (!s_instance) {
        s_instance = new Registry;
    }
    return s_instance;
}

Archive *Registry::createArchive(const QString& mimeType, QObject *volume)
{
    Q_ASSERT(qobject_cast<Volume *>(volume) != 0);

    QObject *o = the()->findArchive(mimeType);

    if (!o)
        return 0;

    ArchiveFactory *factory = qobject_cast<ArchiveFactory *>(o);
    Q_ASSERT(factory);

    return factory->createArchive(mimeType, volume);
}

Codec *Registry::createCodec(const QString& name, QObject *parent)
{
    QObject *o = the()->findCodec(name);

    if (!o)
        return 0;

    CodecFactory *factory = qobject_cast<CodecFactory *>(o);
    Q_ASSERT(factory);

    return factory->createCodec(name, parent);
}

Codec *Registry::createCodec(int id, QObject *parent)
{
    QObject *o = the()->findCodec(id);

    if (!o)
        return 0;

    CodecFactory *factory = qobject_cast<CodecFactory *>(o);
    Q_ASSERT(factory);

    return factory->createCodec(id, parent);
}

Volume *Registry::createVolume(const QString& mime, QObject *parent)
{
    QObject *o = the()->findVolume(mime);

    if (!o)
        return 0;

    VolumeFactory *factory = qobject_cast<VolumeFactory *>(o);
    Q_ASSERT(factory);

    return factory->createVolume(mime, parent);
}
