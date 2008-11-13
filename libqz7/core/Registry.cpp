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
    foreach (QObject *instance, QPluginLoader::staticInstances()) {
        registerPlugin(instance);
    }

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
}

void Registry::registerPlugin(QObject *instance)
{
    if (ArchiveFactory *af = qobject_cast<ArchiveFactory *>(instance)) {
        foreach (const QString& mime, af->archiveMimeTypes()) {
            archivesByMimeType.insert(mime, instance);
        }
    }

    if (CodecFactory *cf = qobject_cast<CodecFactory *>(instance)) {
        foreach (const QString& name, cf->decoderNames()) {
            decodersByName.insert(name, instance);
        }
        foreach (int id, cf->decoderIds()) {
            decodersById.insert(id, instance);
        }
        foreach (const QString& name, cf->encoderNames()) {
            encodersByName.insert(name, instance);
        }
        foreach (int id, cf->encoderIds()) {
            encodersById.insert(id, instance);
        }
    }

    if (VolumeFactory *vf = qobject_cast<VolumeFactory *>(instance)) {
        foreach (const QString& type, vf->volumeMimeTypes()) {
            volumesByMimeType.insert(type, instance);
        }
    }
}

QObject *Registry::findArchive(const QString& mimeType)
{
    return archivesByMimeType.value(mimeType);
}

QObject *Registry::findDecoder(const QString& name)
{
    return decodersByName.value(name);
}

QObject *Registry::findDecoder(int id)
{
    return decodersById.value(id);
}

QObject *Registry::findEncoder(const QString& name)
{
    return encodersByName.value(name);
}

QObject *Registry::findEncoder(int id)
{
    return encodersById.value(id);
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

Archive *Registry::createArchive(const QString& mimeType, Volume *volume)
{
    QObject *o = the()->findArchive(mimeType);

    if (!o)
        return 0;

    ArchiveFactory *factory = qobject_cast<ArchiveFactory *>(o);
    Q_ASSERT(factory);

    return factory->createArchive(mimeType, volume);
}

Codec *Registry::createDecoder(const QString& name, QObject *parent)
{
    QObject *o = the()->findDecoder(name);

    if (!o)
        return 0;

    CodecFactory *factory = qobject_cast<CodecFactory *>(o);
    Q_ASSERT(factory);

    return factory->createDecoder(name, parent);
}

Codec *Registry::createDecoder(int id, QObject *parent)
{
    QObject *o = the()->findDecoder(id);

    if (!o)
        return 0;

    CodecFactory *factory = qobject_cast<CodecFactory *>(o);
    Q_ASSERT(factory);

    return factory->createDecoder(id, parent);
}

Codec *Registry::createEncoder(const QString& name, QObject *parent)
{
    QObject *o = the()->findEncoder(name);

    if (!o)
        return 0;

    CodecFactory *factory = qobject_cast<CodecFactory *>(o);
    Q_ASSERT(factory);

    return factory->createEncoder(name, parent);
}

Codec *Registry::createEncoder(int id, QObject *parent)
{
    QObject *o = the()->findEncoder(id);

    if (!o)
        return 0;

    CodecFactory *factory = qobject_cast<CodecFactory *>(o);
    Q_ASSERT(factory);

    return factory->createEncoder(id, parent);
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

} // namespace qz7
