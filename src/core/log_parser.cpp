#include "include/core/log_parser.h"
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>

LogParser::LogParser(QObject *parent)
    : QObject(parent)
    , m_busy(false)
    , m_lastCeiling(0.0)
{
}

bool LogParser::importFile(const QString &filePath)
{
    qDebug() << "LogParser::importFile called with path:" << filePath;
    
    if (m_busy) {
        m_lastError = tr("Already processing a file");
        emit errorOccurred(m_lastError);
        return false;
    }
    
    m_busy = true;
    emit busyChanged();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Could not open file: %1 - Error: %2").arg(filePath).arg(file.errorString());
        qDebug() << "File open error:" << m_lastError;
        emit errorOccurred(m_lastError);
        m_busy = false;
        emit busyChanged();
        return false;
    }
    
    QList<DiveData*> dives;
    bool success = false;
    
    // Check file extension to determine format
    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();
    
    qDebug() << "File extension:" << ext;
    
    if (ext == "xml" || ext == "ssrf") {
        // Make sure we're at the beginning of the file
        file.seek(0);
        qDebug() << "Starting to parse Subsurface XML file";
        success = parseSubsurfaceXML(file, dives);
    } else {
        m_lastError = tr("Unsupported file format: %1").arg(ext);
        qDebug() << "Unsupported file format:" << ext;
        emit errorOccurred(m_lastError);
        success = false;
    }
    
    file.close();
    
    if (success) {
        if (dives.size() == 1) {
            qDebug() << "Emitting diveImported signal for dive:" << dives.first()->diveName();
            emit diveImported(dives.first());
        } else if (dives.size() > 1) {
            qDebug() << "Emitting multipleImported signal with" << dives.size() << "dives";
            emit multipleImported(dives);
        } else {
            m_lastError = tr("No dives found in file");
            qDebug() << "No dives found in file";
            emit errorOccurred(m_lastError);
            success = false;
        }
    }
    
    m_busy = false;
    emit busyChanged();
    
    return success;
}

bool LogParser::importDive(const QString &filePath, int diveNumber)
{
    if (m_busy) {
        m_lastError = tr("Already processing a file");
        emit errorOccurred(m_lastError);
        return false;
    }
    
    m_busy = true;
    emit busyChanged();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Could not open file: %1").arg(file.errorString());
        emit errorOccurred(m_lastError);
        m_busy = false;
        emit busyChanged();
        return false;
    }
    
    QList<DiveData*> dives;
    bool success = false;
    
    // Check file extension to determine format
    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();
    
    if (ext == "xml" || ext == "ssrf") {
        success = parseSubsurfaceXML(file, dives, diveNumber);
    } else {
        m_lastError = tr("Unsupported file format: %1").arg(ext);
        emit errorOccurred(m_lastError);
        success = false;
    }
    
    file.close();
    
    if (success && !dives.isEmpty()) {
        emit diveImported(dives.first());
    } else if (success) {
        m_lastError = tr("Dive number %1 not found in file").arg(diveNumber);
        emit errorOccurred(m_lastError);
        success = false;
    }
    
    m_busy = false;
    emit busyChanged();
    
    return success;
}

QList<QString> LogParser::getDiveList(const QString &filePath)
{
    QList<QString> result;
    
    if (m_busy) {
        m_lastError = tr("Already processing a file");
        emit errorOccurred(m_lastError);
        return result;
    }
    
    m_busy = true;
    emit busyChanged();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Could not open file: %1").arg(file.errorString());
        emit errorOccurred(m_lastError);
        m_busy = false;
        emit busyChanged();
        return result;
    }
    
    // Check file extension to determine format
    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();
    
    if (ext == "xml" || ext == "ssrf") {
        QXmlStreamReader xml(&file);
        
        while (!xml.atEnd() && !xml.hasError()) {
            QXmlStreamReader::TokenType token = xml.readNext();
            
            if (token == QXmlStreamReader::StartElement && xml.name() == "dive") {
                QString diveDate;
                QString diveTime;
                QString diveLocation;
                
                // Get dive number
                QXmlStreamAttributes attrs = xml.attributes();
                if (attrs.hasAttribute("number")) {
                    QString number = attrs.value("number").toString();
                    
                    // Get date and time
                    if (attrs.hasAttribute("date")) {
                        diveDate = attrs.value("date").toString();
                    }
                    if (attrs.hasAttribute("time")) {
                        diveTime = attrs.value("time").toString();
                    }
                    
                    // Read dive location
                    while (!xml.atEnd() && !(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "dive")) {
                        if (xml.tokenType() == QXmlStreamReader::StartElement && xml.name() == "location") {
                            diveLocation = xml.readElementText();
                            break;
                        }
                        xml.readNext();
                    }
                    
                    // Create the dive entry for the list
                    QString entry = tr("Dive #%1").arg(number);
                    if (!diveDate.isEmpty()) {
                        entry += " - " + diveDate;
                    }
                    if (!diveTime.isEmpty()) {
                        entry += " " + diveTime;
                    }
                    if (!diveLocation.isEmpty()) {
                        entry += " at " + diveLocation;
                    }
                    
                    result.append(entry);
                }
            }
        }
        
        if (xml.hasError()) {
            m_lastError = tr("XML parsing error: %1").arg(xml.errorString());
            emit errorOccurred(m_lastError);
            result.clear();
        }
    } else {
        m_lastError = tr("Unsupported file format: %1").arg(ext);
        emit errorOccurred(m_lastError);
    }
    
    file.close();
    
    m_busy = false;
    emit busyChanged();
    
    return result;
}

bool LogParser::parseSubsurfaceXML(QFile &file, QList<DiveData*> &result, int specificDive)
{
    qDebug() << "Starting to parse Subsurface XML file";
    QXmlStreamReader xml(&file);
    
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        
        if (token == QXmlStreamReader::StartElement && xml.name() == "dive") {
            qDebug() << "Found dive element";
            // Check if we're looking for a specific dive
            if (specificDive != -1) {
                QXmlStreamAttributes attrs = xml.attributes();
                if (attrs.hasAttribute("number")) {
                    bool ok;
                    int number = attrs.value("number").toInt(&ok);
                    if (!ok || number != specificDive) {
                        // Skip this dive and continue with the next one
                        continue;
                    }
                }
            }
            
            DiveData* dive = parseDiveElement(xml);
            if (dive) {
                qDebug() << "Successfully parsed dive:" << dive->diveName();
                result.append(dive);
                
                // If we found the specific dive we were looking for, we can stop
                if (specificDive != -1) {
                    break;
                }
            }
        }
    }
    
    if (xml.hasError()) {
        m_lastError = tr("XML parsing error: %1").arg(xml.errorString());
        qDebug() << "XML parsing error:" << xml.errorString();
        emit errorOccurred(m_lastError);
        
        // Clean up any dives we've created so far
        qDeleteAll(result);
        result.clear();
        
        return false;
    }
    
    qDebug() << "Finished parsing XML file, found" << result.size() << "dives";
    return true;
}

DiveData* LogParser::parseDiveElement(QXmlStreamReader &xml)
{
    // Create a new dive data object
    DiveData* dive = new DiveData(this);
    
    // Reset the initial cylinder pressures map
    m_initialCylinderPressures.clear();
    
    // Parse dive attributes
    QXmlStreamAttributes attrs = xml.attributes();
    
    // Get dive number and name
    if (attrs.hasAttribute("number")) {
        QString number = attrs.value("number").toString();
        dive->setDiveName(tr("Dive #%1").arg(number));
    }
    
    // Get date and time
    QDateTime diveDateTime;
    if (attrs.hasAttribute("date")) {
        QString dateStr = attrs.value("date").toString();
        if (attrs.hasAttribute("time")) {
            QString timeStr = attrs.value("time").toString();
            diveDateTime = QDateTime::fromString(dateStr + " " + timeStr, "yyyy-MM-dd hh:mm:ss");
        } else {
            diveDateTime = QDateTime::fromString(dateStr, "yyyy-MM-dd");
        }
        dive->setStartTime(diveDateTime);
    }
    
    qDebug() << "Parsing dive element for" << dive->diveName();
    
    // Parse dive elements - continue until we reach the end of the dive element
    int sampleCount = 0;  // Track how many samples we've parsed - ADDED THIS LINE
    
    // We need to continue reading until we hit the end of the dive element
    while (!xml.atEnd()) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "dive") {
            break;
        }
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString elementName = xml.name().toString();
            
            if (elementName == "location") {
                dive->setLocation(xml.readElementText());
            } else if (elementName == "cylinder") {
                // Parse cylinder information
                parseCylinderElement(xml, dive);
            } else if (elementName == "divecomputer") {
                // In Subsurface, samples are inside the divecomputer element
                // We need to process the divecomputer element without consuming its end tag
                parseDiveComputerElement(xml, dive, sampleCount);
            }
        }
    }
    
    // Ensure we have at least one dummy point with all cylinders initialized if no samples were found
    if (dive->allDataPoints().isEmpty() && dive->cylinderCount() > 0) {
        DiveDataPoint initialPoint;
        initialPoint.timestamp = 0.0;
        
        // Initialize all cylinders with their default pressures
        for (int i = 0; i < dive->cylinderCount(); i++) {
            double initialPressure = m_initialCylinderPressures.value(i, 0.0);
            if (initialPressure > 0.0) {
                initialPoint.addPressure(initialPressure, i);
            }
        }
        
        dive->addDataPoint(initialPoint);
    }
    
    qDebug() << "Finished parsing dive element. Total data points:" << dive->allDataPoints().size();
    return dive;
}

void LogParser::parseCylinderElement(QXmlStreamReader &xml, DiveData* dive)
{
    // Get cylinder attributes
    QXmlStreamAttributes attrs = xml.attributes();
    CylinderInfo cylinder;
    
    // Parse cylinder size
    if (attrs.hasAttribute("size")) {
        QString sizeStr = attrs.value("size").toString();
        QRegularExpression sizeRe("(\\d+\\.?\\d*)\\s+l");
        QRegularExpressionMatch match = sizeRe.match(sizeStr);
        
        if (match.hasMatch()) {
            cylinder.size = match.captured(1).toDouble();
        }
    }
    
    // Parse working pressure
    if (attrs.hasAttribute("workpressure")) {
        QString pressureStr = attrs.value("workpressure").toString();
        QRegularExpression pressureRe("(\\d+\\.?\\d*)\\s+bar");
        QRegularExpressionMatch match = pressureRe.match(pressureStr);
        
        if (match.hasMatch()) {
            cylinder.workPressure = match.captured(1).toDouble();
        }
    }
    
    // Parse description
    if (attrs.hasAttribute("description")) {
        cylinder.description = attrs.value("description").toString();
    }
    
    // Parse O2 percentage
    if (attrs.hasAttribute("o2")) {
        QString o2Str = attrs.value("o2").toString();
        QRegularExpression o2Re("(\\d+\\.?\\d*)\\s*%");
        QRegularExpressionMatch match = o2Re.match(o2Str);
        
        if (match.hasMatch()) {
            cylinder.o2Percent = match.captured(1).toDouble();
        }
    }
    
    // Parse Helium percentage for trimix
    if (attrs.hasAttribute("he")) {
        QString heStr = attrs.value("he").toString();
        QRegularExpression heRe("(\\d+\\.?\\d*)\\s*%");
        QRegularExpressionMatch match = heRe.match(heStr);
        
        if (match.hasMatch()) {
            cylinder.hePercent = match.captured(1).toDouble();
        }
    }
    
    // Parse start pressure
    if (attrs.hasAttribute("start")) {
        QString startStr = attrs.value("start").toString();
        QRegularExpression startRe("(\\d+\\.?\\d*)\\s+bar");
        QRegularExpressionMatch match = startRe.match(startStr);
        
        if (match.hasMatch()) {
            cylinder.startPressure = match.captured(1).toDouble();
        }
    }
    
    // Parse end pressure
    if (attrs.hasAttribute("end")) {
        QString endStr = attrs.value("end").toString();
        QRegularExpression endRe("(\\d+\\.?\\d*)\\s+bar");
        QRegularExpressionMatch match = endRe.match(endStr);
        
        if (match.hasMatch()) {
            cylinder.endPressure = match.captured(1).toDouble();
        }
    }
    
    // Determine initial pressure (start pressure if available, otherwise work pressure)
    double initialPressure = 0.0;
    if (cylinder.startPressure > 0.0) {
        initialPressure = cylinder.startPressure;
    } else if (cylinder.workPressure > 0.0) {
        initialPressure = cylinder.workPressure;
    }
    
    // Add the cylinder to the dive
    int cylinderIndex = dive->cylinderCount();
    dive->addCylinder(cylinder);
    
    // Initialize the lastPressures map with this cylinder's initial pressure
    if (initialPressure > 0.0) {
        // Store this initial pressure as the first value for this tank
        // Note: We'll need to make this available to parseSampleElement somehow
        // This could be done by adding it to a member variable in LogParser
        m_initialCylinderPressures[cylinderIndex] = initialPressure;
    }
    
    qDebug() << "Parsed cylinder:" << cylinder.description 
             << "Size:" << cylinder.size << "l"
             << "Gas mix:" << cylinder.o2Percent << "% O2" 
             << (cylinder.hePercent > 0 ? QString::number(cylinder.hePercent) + "% He" : "")
             << "Initial pressure:" << initialPressure << "bar";
    
    // Skip to the end of this element
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "cylinder")) {
        xml.readNext();
        if (xml.atEnd()) break;
    }
}

void LogParser::parseDiveComputerElement(QXmlStreamReader &xml, DiveData* dive, int &sampleCount)
{
    qDebug() << "Parsing divecomputer element";
    
    // Variables to remember the last known values
    double lastTemperature = 0.0;
    double lastNDL = 0.0;
    double lastTTS = 0.0;

    // Keep track of the last pressure for each tank - use map for sparse storage
    // Initialize last pressures with the initial values for all cylinders
    QMap<int, double> lastPressures;
    for (int i = 0; i < dive->cylinderCount(); i++) {
        double initialPressure = m_initialCylinderPressures.value(i, 0.0);
        if (initialPressure > 0.0) {
            lastPressures[i] = initialPressure;
        }
    }
    
    // Process all elements within the divecomputer element
    while (!xml.atEnd()) {
        xml.readNext();
        
        if (xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "divecomputer") {
            break;  // Exit when we reach the end of divecomputer
        }
        
        if (xml.tokenType() == QXmlStreamReader::StartElement) {
            QString elementName = xml.name().toString();
            
            if (elementName == "sample") {
                // Process this sample
                parseSampleElement(xml, dive, lastTemperature, lastNDL, lastTTS, lastPressures);
                sampleCount++;
                
                // Log every 10th sample for debugging
                if (sampleCount % 10 == 0) {
                    qDebug() << "Parsed" << sampleCount << "samples";
                }
            } else if (elementName == "temperature") {
                // Handle temperature element at divecomputer level
                QXmlStreamAttributes attrs = xml.attributes();
                if (attrs.hasAttribute("water")) {
                    QString tempStr = attrs.value("water").toString();
                    QRegularExpression tempRe("(\\d+\\.?\\d*)\\s+C");
                    QRegularExpressionMatch match = tempRe.match(tempStr);
                    
                    if (match.hasMatch()) {
                        lastTemperature = match.captured(1).toDouble();
                    }
                }
                // Skip to the end of this element
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "temperature")) {
                    xml.readNext();
                    if (xml.atEnd()) break;
                }
            }
        }
    }
    
    qDebug() << "Finished parsing divecomputer element with" << sampleCount << "samples";
}

void LogParser::parseSampleElement(QXmlStreamReader &xml, DiveData* dive, double &lastTemperature, double &lastNDL, double &lastTTS, QMap<int, double> &lastPressures)
{
    QXmlStreamAttributes attrs = xml.attributes();
    
    DiveDataPoint point;
    bool hasData = false;
    bool inDeco = false;
    
    // Get time - Subsurface uses format "mm:ss min"
    if (attrs.hasAttribute("time")) {
        QString timeStr = attrs.value("time").toString();
        // Extract minutes and seconds from "mm:ss min" format
        QRegularExpression timeRe("(\\d+):(\\d+)\\s+min");
        QRegularExpressionMatch match = timeRe.match(timeStr);
        
        if (match.hasMatch()) {
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            point.timestamp = minutes * 60 + seconds;
            hasData = true;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double time = timeStr.toDouble(&ok);
            if (ok) {
                point.timestamp = time;
                hasData = true;
            }
        }
    }
    
    // Get depth - Subsurface uses format "xx.x m"
    if (attrs.hasAttribute("depth")) {
        QString depthStr = attrs.value("depth").toString();
        QRegularExpression depthRe("(\\d+\\.?\\d*)\\s+m");
        QRegularExpressionMatch match = depthRe.match(depthStr);
        
        if (match.hasMatch()) {
            point.depth = match.captured(1).toDouble();
            hasData = true;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double depth = depthStr.toDouble(&ok);
            if (ok) {
                point.depth = depth;
                hasData = true;
            }
        }
    }
    
    // Get temperature - Subsurface uses format "xx.x C"
    if (attrs.hasAttribute("temp")) {
        QString tempStr = attrs.value("temp").toString();
        QRegularExpression tempRe("(\\d+\\.?\\d*)\\s+C");
        QRegularExpressionMatch match = tempRe.match(tempStr);
        
        if (match.hasMatch()) {
            point.temperature = match.captured(1).toDouble();
            lastTemperature = point.temperature;  // Update last known temperature
            hasData = true;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double temp = tempStr.toDouble(&ok);
            if (ok) {
                point.temperature = temp;
                lastTemperature = temp;  // Update last known temperature
                hasData = true;
            }
        }
    } else {
        // Use the last known temperature if available
        if (lastTemperature > 0.0) {
            point.temperature = lastTemperature;
        }
    }
    
    // Process pressure attributes (standard and numbered for multiple tanks)

    // First check for the standard "pressure" attribute (single tank)
    if (attrs.hasAttribute("pressure")) {
        QString pressureStr = attrs.value("pressure").toString();
        QRegularExpression pressureRe("(\\d+\\.?\\d*)\\s+bar");
        QRegularExpressionMatch match = pressureRe.match(pressureStr);
        
        if (match.hasMatch()) {
            double pressure = match.captured(1).toDouble();
            point.addPressure(pressure, 0); // Add to first tank (index 0)
            hasData = true;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double pressure = pressureStr.toDouble(&ok);
            if (ok) {
                point.addPressure(pressure, 0);
                hasData = true;
            }
        }
    }

    // Now check for numbered pressure attributes (multiple tanks)
    for (int i = 0; i < 10; i++) { // Check up to 10 tanks (reasonable limit)
        QString pressureAttr = QString("pressure%1").arg(i);
        
        if (attrs.hasAttribute(pressureAttr)) {
            QString pressureStr = attrs.value(pressureAttr).toString();
            qDebug() << "Found tank" << i << "pressure:" << pressureStr;
            QRegularExpression pressureRe("(\\d+\\.?\\d*)\\s+bar");
            QRegularExpressionMatch match = pressureRe.match(pressureStr);
            
            if (match.hasMatch()) {
                double pressure = match.captured(1).toDouble();
                point.addPressure(pressure, i);
                lastPressures[i] = pressure;  // Update last known pressure for this tank
                hasData = true;
                
                qDebug() << "Found tank" << i << "pressure:" << pressure << "bar";
            } else {
                // Try direct numeric parsing as fallback
                bool ok;
                double pressure = pressureStr.toDouble(&ok);
                if (ok) {
                    point.addPressure(pressure, i);
                    lastPressures[i] = pressure;
                    hasData = true;
                }
            }
        }
    }

    // Apply last known pressures for ALL cylinders declared in the dive
    // This ensures every cylinder has a pressure value in every sample
    for (int i = 0; i < dive->cylinderCount(); i++) {
        // Check if this tank already has a pressure set for this sample
        bool pressureSet = false;
        if (i == 0) {
            pressureSet = attrs.hasAttribute("pressure") || attrs.hasAttribute("pressure0");
        } else {
            pressureSet = attrs.hasAttribute(QString("pressure%1").arg(i));
        }
        
        // If not set explicitly in this sample, use the last known value
        if (!pressureSet && lastPressures.contains(i)) {
            point.addPressure(lastPressures[i], i);
        }
    }
    
    // Check for in_deco flag
    if (attrs.hasAttribute("in_deco")) {
        QString decoStr = attrs.value("in_deco").toString();
        inDeco = (decoStr == "1" || decoStr.toLower() == "true");
    }
    
    // Get TTS (Time To Surface) - Subsurface uses format "mm:ss min"
    if (attrs.hasAttribute("tts")) {
        QString ttsStr = attrs.value("tts").toString();
        QRegularExpression ttsRe("(\\d+):(\\d+)\\s+min");
        QRegularExpressionMatch match = ttsRe.match(ttsStr);
        
        if (match.hasMatch()) {
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            point.tts = minutes + seconds / 60.0;
            lastTTS = point.tts;  // Update last known TTS
            hasData = true;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double tts = ttsStr.toDouble(&ok);
            if (ok) {
                point.tts = tts;
                lastTTS = tts;  // Update last known TTS
                hasData = true;
            }
        }
    } else {
        // Use the last known TTS if available
        if (lastTTS > 0.0) {
            point.tts = lastTTS;
        }
    }
    
    // Get NDL - Subsurface uses format "xx:xx min"
    if (attrs.hasAttribute("ndl")) {
        QString ndlStr = attrs.value("ndl").toString();
        QRegularExpression ndlRe("(\\d+):(\\d+)\\s+min");
        QRegularExpressionMatch match = ndlRe.match(ndlStr);
        
        if (match.hasMatch()) {
            int minutes = match.captured(1).toInt();
            int seconds = match.captured(2).toInt();
            point.ndl = minutes + seconds / 60.0;
            lastNDL = point.ndl;  // Update last known NDL
            hasData = true;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double ndl = ndlStr.toDouble(&ok);
            if (ok) {
                point.ndl = ndl;
                lastNDL = ndl;  // Update last known NDL
                hasData = true;
            }
        }
    } else {
        // Use the last known NDL if available
        if (lastNDL >= 0.0) {
            point.ndl = lastNDL;
        }
    }
    
    // When in deco, NDL should be 0 and TTS should be positive
    if (inDeco) {
        point.ndl = 0.0;
        lastNDL = 0.0;
        
        // Ensure TTS is set when in deco
        if (point.tts <= 0.0 && lastTTS > 0.0) {
            point.tts = lastTTS;
        } else if (point.tts <= 0.0) {
            // If we don't have a lastTTS, set a minimum value
            point.tts = 1.0;
            lastTTS = 1.0;
        }
    }

    if (attrs.hasAttribute("stopdepth")) {
        QString stopDepthStr = attrs.value("stopdepth").toString();
        QRegularExpression stopDepthRe("(\\d+\\.?\\d*)\\s+m");
        QRegularExpressionMatch match = stopDepthRe.match(stopDepthStr);
        
        if (match.hasMatch()) {
            point.ceiling = match.captured(1).toDouble();
            m_lastCeiling = point.ceiling; // Store for future samples
            qDebug() << "Parsed stopdepth:" << point.ceiling << "m for time:" << point.timestamp;
        } else {
            // Try direct numeric parsing as fallback
            bool ok;
            double stopDepth = stopDepthStr.toDouble(&ok);
            if (ok) {
                point.ceiling = stopDepth;
                m_lastCeiling = point.ceiling; // Store for future samples
                qDebug() << "Parsed stopdepth (numeric):" << point.ceiling << "m for time:" << point.timestamp;
            }
        }
    } else {
        // If no stopdepth in this sample, use the last known ceiling value
        point.ceiling = m_lastCeiling;
    }
    
    // If we have any valid data, add the point to the dive
    if (hasData) {
        static int sampleCount = 0;
        sampleCount++;
        
        // Print debug info for every 10th sample
        if (sampleCount <= 5 || sampleCount % 20 == 0) {
            qDebug() << "Sample #" << sampleCount 
                     << "time=" << point.timestamp 
                     << "depth=" << point.depth
                     << "temp=" << point.temperature << "(lastTemp=" << lastTemperature << ")"
                     << "ndl=" << point.ndl << "(lastNDL=" << lastNDL << ")"
                     << "tts=" << point.tts << "(lastTTS=" << lastTTS << ")"
                     << "in_deco=" << inDeco;
            // Debug tank pressures
            for (int i = 0; i < point.tankCount(); i++) {
                qDebug() << "  Tank" << i << "pressure=" << point.getPressure(i) 
                         << "(last=" << lastPressures.value(i, 0.0) << ")";
            }
        }
        
        dive->addDataPoint(point);
    }
    
    // Skip to the end of the sample element
    while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "sample")) {
        xml.readNext();
        if (xml.atEnd()) break;
    }
}