/*
Copyright 2008 Nick Shaforostoff <shaforostoff@kde.ru>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gettextstorage.h"

#include "gettextheader.h"
#include "catalogitem_private.h"
#include "gettextimport.h"
#include "gettextexport.h"

#include "project.h"

#include "version.h"
#include "prefs_lokalize.h"

#include <QProcess>
#include <QString>
#include <QMap>

#include <kdebug.h>

// static QString GNUPluralForms(const QString& lang);

using namespace GettextCatalog;

GettextStorage::GettextStorage()
 : CatalogStorage()
{
}


GettextStorage::~GettextStorage()
{
}

//BEGIN OPEN/SAVE

bool GettextStorage::load(QIODevice* device/*, bool readonly*/)
{
    //GettextImportPlugin importer=GettextImportPlugin(readonly?(new ExtraDataSaver()):(new ExtraDataListSaver()));
    GettextImportPlugin importer;
    ConversionStatus status = OK;
    status = importer.open(device,this);

    //for langs with more than 2 forms
    //we create any form-entries additionally needed
    uint i=0;
    uint lim=size();
    while (i<lim)
    {
        CatalogItem& item=m_entries[i];
        if (item.isPlural()
            && item.msgstrPlural().count()<m_numberOfPluralForms
           )
        {
            QVector<QString> msgstr(item.msgstrPlural());
            while (msgstr.count()<m_numberOfPluralForms)
                msgstr.append(QString());
            item.setMsgstr(msgstr);
        }
        ++i;

    }
    
    //qCompress(m_storage->m_catalogExtraData.join("\n\n").toUtf8(),9);

    return status==OK;
}

bool GettextStorage::save(QIODevice* device)
{
    QString header=m_header.msgstr();
    QString comment=m_header.comment();
    updateHeader(header,
                 comment,
                 m_langCode,
                 m_numberOfPluralForms,
                 m_url.fileName(),
                 m_generatedFromDocbook,
                 /*forSaving*/true);
    m_header.setMsgstr(header);
    m_header.setComment(comment);

    GettextExportPlugin exporter(m_maxLineLength>70?m_maxLineLength:-1, m_trailingNewLines);// this is kinda hackish...

    ConversionStatus status = OK;
    status = exporter.save(device/*x-gettext-translation*/,this);

    return status==OK;
}

//END OPEN/SAVE

//BEGIN STORAGE TRANSLATION

int GettextStorage::size() const
{
    return m_entries.size();
}

void GettextStorage::clear()
{
}

bool GettextStorage::isEmpty() const
{
    return m_entries.isEmpty();
}

//flat-model interface (ignores XLIFF grouping)
QString GettextStorage::source(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).msgid(pos.form);
}
QString GettextStorage::target(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).msgstr(pos.form);
}


void GettextStorage::targetDelete(const DocPosition& pos, int count)
{
    m_entries[pos.entry].d->_msgstrPlural[pos.form].remove(pos.offset, count);
}
void GettextStorage::targetInsert(const DocPosition& pos, const QString& arg)
{
    m_entries[pos.entry].d->_msgstrPlural[pos.form].insert(pos.offset, arg);
}
void GettextStorage::setTarget(const DocPosition& pos, const QString& arg)
{
    m_entries[pos.entry].d->_msgstrPlural[pos.form]=arg;
}

QStringList GettextStorage::sourceAllForms(const DocPosition& pos) const
{
    return m_entries[pos.entry].d->_msgidPlural.toList();
}
QStringList GettextStorage::targetAllForms(const DocPosition& pos) const
{
    return m_entries[pos.entry].d->_msgstrPlural.toList();
}

//DocPosition.form - number of <note>
QString GettextStorage::note(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).comment();
}
int GettextStorage::noteCount(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).comment().isEmpty()?0:1;
}

//DocPosition.form - number of <context>
QString GettextStorage::context(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).msgctxt();
}
int GettextStorage::contextCount(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).msgctxt().isEmpty()?0:1;
}

QStringList GettextStorage::matchData(const DocPosition& pos) const
{
    QString ctxt=m_entries.at(pos.entry).msgctxt();
    if (ctxt.isEmpty())
        return QStringList();

    //KDE-specific
    //Splits @info:whatsthis and actual note
/*    if (ctxt.startsWith('@') && ctxt.contains(' '))
    {
        QStringList result(ctxt.section(' ',0,0,QString::SectionSkipEmpty));
        result<<ctxt.section(' ',1,-1,QString::SectionSkipEmpty);
        return result;
    }*/
    return QStringList(ctxt);
}

QString GettextStorage::id(const DocPosition& pos) const
{
    //entries in gettext format may be non-unique
    //only if their msgctxts are different

    QString result=source(pos);
    result+=m_entries.at(pos.entry).msgctxt();
    return result;
/*    QByteArray result=source(pos).toUtf8();
    result+=m_entries.at(pos.entry).msgctxt().toUtf8();
    return QString qChecksum(result);*/
}

bool GettextStorage::isPlural(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).isPlural();
}

bool GettextStorage::isApproved(const DocPosition& pos) const
{
    return !m_entries.at(pos.entry).isFuzzy();
}
void GettextStorage::setApproved(const DocPosition& pos, bool approved)
{
    if (approved)
        m_entries[pos.entry].unsetFuzzy();
    else
        m_entries[pos.entry].setFuzzy();
}

bool GettextStorage::isUntranslated(const DocPosition& pos) const
{
    return m_entries.at(pos.entry).isUntranslated();
}

//END STORAGE TRANSLATION




//called from importer
bool GettextStorage::setHeader(const CatalogItem& newHeader)
{
   if(newHeader.isValid())
   {
      // normalize the values - ensure every key:value pair is only on a single line
      QString values = newHeader.msgstr();
      values.remove ("\n");
      values.replace ("\\n", "\\n\n");
//       kDebug () << "Normalized header: " << values;
      QString comment=newHeader.comment();

      updateHeader(values,
                   comment,
                   m_langCode,
                   m_numberOfPluralForms,
                   m_url.fileName(),
                   m_generatedFromDocbook,
                  /*forSaving*/true);
      m_header=newHeader;
      m_header.setComment(comment);
      m_header.setMsgstr(values);

//       setClean(false);
      //emit signalHeaderChanged();

      return true;
   }
   kWarning () << "header Not valid";
   return false;
}



