/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtDebug>

#include "qacceltreeresourceloader_p.h"
#include "qnetworkaccessdelegator_p.h"

#include "Global.h"
#include "TestBaseLine.h"
#include "TestGroup.h"

#include "XSLTTestSuiteHandler.h"

using namespace QPatternistSDK;

extern QNetworkAccessManager s_networkManager;

XSLTTestSuiteHandler::XSLTTestSuiteHandler(const QUrl &catalogFile) : m_ts(0)
                                                                    , m_tc(0)
                                                                    , m_baseLine(0)
                                                                    , m_catalogFile(catalogFile)
                                                                    , m_removeTestcase(false)
{
    const QPatternist::NetworkAccessDelegator::Ptr networkDelegator(new QPatternist::NetworkAccessDelegator(&s_networkManager, &s_networkManager));

    m_resourceLoader = QPatternist::ResourceLoader::Ptr(new QPatternist::AccelTreeResourceLoader(Global::namePool(),
                                                                                                 networkDelegator));
    Q_ASSERT(!m_catalogFile.isRelative());
}

bool XSLTTestSuiteHandler::startElement(const QString &namespaceURI,
                                        const QString &localName,
                                        const QString &/*qName*/,
                                        const QXmlAttributes &atts)
    {
    if(namespaceURI != Global::xsltsCatalogNS)
        return true;

    /* The elements are handled roughly in the order of highest occurrence in the catalog file. */
    if(localName == QLatin1String("testcase"))
    {
        /* We pass m_ts temporarily, and change it later. */
        m_tc = new XQTSTestCase(TestCase::Standard, 0, QXmlQuery::XSLT20);

        m_currentQueryPath = m_queryOffset.resolved(QUrl(atts.value(QLatin1String("FilePath"))));
        m_currentBaselinePath = m_baselineOffset.resolved(QUrl(atts.value(QLatin1String("FilePath"))));
    }
    else if(localName == QLatin1String("stylesheet"))
        m_tc->setQueryPath(m_currentQueryPath.resolved(atts.value(QLatin1String("file"))));
    else if(localName == QLatin1String("error"))
    {
        m_baseLine = new TestBaseLine(TestBaseLine::ExpectedError);
        m_baseLine->setDetails(atts.value(QLatin1String("error-id")));
        m_tc->addBaseLine(m_baseLine);
    }
    else if(localName == QLatin1String("testcases"))
    {
        m_ts = new TestSuite();
        m_ts->setVersion(atts.value(QLatin1String("testSuiteVersion")));

        m_queryOffset           = m_catalogFile.resolved(atts.value(QLatin1String("InputOffsetPath")));
        m_baselineOffset        = m_catalogFile.resolved(atts.value(QLatin1String("ResultOffsetPath")));
        m_sourceOffset          = m_catalogFile.resolved(atts.value(QLatin1String("InputOffsetPath")));
    }
    else if(localName == QLatin1String("source-document"))
    {
        if(atts.value(QLatin1String("role")) == QLatin1String("principal"))
            m_tc->setContextItemSource(m_sourceOffset.resolved(QUrl(atts.value(QLatin1String("file")))));
    }
    else if(localName == QLatin1String("result-document"))
    {
        m_baseLine = new TestBaseLine(TestBaseLine::identifierFromString(atts.value(QLatin1String("type"))));
        m_baseLine->setDetails(m_currentBaselinePath.resolved(atts.value(QLatin1String("file"))).toString());
        m_tc->addBaseLine(m_baseLine);
    }
    else if(localName == QLatin1String("discretionary-feature"))
    {
        const QString feature(atts.value(QLatin1String("name")));

        m_removeTestcase = feature == QLatin1String("schema_aware") ||
                           feature == QLatin1String("namespace_axis") ||
                           feature == QLatin1String("disabling_output_escaping") ||
                           feature == QLatin1String("XML_1.1");
    }
    else if(localName == QLatin1String("discretionary-choice"))
    {
        m_baseLine = new TestBaseLine(TestBaseLine::ExpectedError);
        m_baseLine->setDetails(atts.value(QLatin1String("name")));
        m_tc->addBaseLine(m_baseLine);
        const QString feature(atts.value(QLatin1String("name")));

        m_removeTestcase = feature == QLatin1String("schema_aware") ||
                           feature == QLatin1String("namespace_axis") ||
                           feature == QLatin1String("disabling_output_escaping") ||
                           feature == QLatin1String("XML_1.1");
    }
    else if(localName == QLatin1String("entry-named-template"))
    {
        const QString name(atts.value(QLatin1String("qname")));

        if(!name.contains(QLatin1Char(':')))
        {
            // TODO do namespace processing
            const QXmlName complete(Global::namePool()->allocateQName(QString(), name));

            m_tc->setInitialTemplateName(complete);
        }
    }

    return true;
}

TestGroup *XSLTTestSuiteHandler::containerFor(const QString &name)
{
    TestGroup *& c = m_containers[name];

    if(!c)
    {
        c = new TestGroup(m_ts);
        c->setTitle(name);
        Q_ASSERT(c);
        m_ts->appendChild(c);
    }

    return c;
}

bool XSLTTestSuiteHandler::endElement(const QString &namespaceURI,
                                      const QString &localName,
                                      const QString &/*qName*/)
{
    if(namespaceURI != Global::xsltsCatalogNS)
        return true;

    /* The elements are handled roughly in the order of highest occurrence in the catalog file. */
    if(localName == QLatin1String("description"))
    {
        if(m_tc)
        {
            /* We're inside a <testcase>, so the <description> belongs
             * to the testcase. */
            m_tc->setDescription(m_ch.simplified());
        }
    }
    else if(localName == QLatin1String("testcase"))
    {
        Q_ASSERT(m_tc->baseLines().count() >= 1);
        Q_ASSERT(m_resourceLoader);
        // TODO can this be removed?
        m_tc->setExternalVariableLoader(QPatternist::ExternalVariableLoader::Ptr
                                                (new ExternalSourceLoader(m_tcSourceInputs,
                                                                          m_resourceLoader)));
        m_tcSourceInputs.clear();

        if(!m_removeTestcase)
        {
            /*
            TestContainer *const container = containerFor(m_currentCategory);
            delete m_tc;
            container->removeLast();
            */
            TestContainer *const container = containerFor(m_currentCategory);
            m_tc->setParent(container);
            Q_ASSERT(m_tc);
            container->appendChild(m_tc);
        }

        m_tc = 0;
        m_removeTestcase = false;
    }
    else if(localName == QLatin1String("name"))
        m_tc->setName(m_ch);
    else if(localName == QLatin1String("creator"))
        m_tc->setCreator(m_ch);
    else if(localName == QLatin1String("contextItem"))
        m_contextItemSource = m_ch;
    else if(localName == QLatin1String("category"))
        m_currentCategory = m_ch;

    return true;
}

bool XSLTTestSuiteHandler::characters(const QString &ch)
{
    m_ch = ch;
    return true;
}

TestSuite *XSLTTestSuiteHandler::testSuite() const
{
    return m_ts;
}

// vim: et:ts=4:sw=4:sts=4
