#include "ClassroomClient.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

ClassroomClient::ClassroomClient(QObject *parent)
    : QObject(parent)
{}

void ClassroomClient::setAccessToken(const QString &accessToken)
{
    m_accessToken = accessToken.trimmed();
}

void ClassroomClient::setUseSampleData(bool enabled)
{
    m_useSampleData = enabled;
}

bool ClassroomClient::useSampleData() const
{
    return m_useSampleData;
}

void ClassroomClient::setSampleDataPath(const QString &path)
{
    m_sampleDataPath = path;
}

void ClassroomClient::fetchCourses()
{
    if (m_useSampleData) {
        fetchCoursesFromSample();
    } else {
        fetchCoursesFromApi();
    }
}

void ClassroomClient::fetchCourseWork(const QString &courseId)
{
    if (m_useSampleData) {
        fetchCourseWorkFromSample(courseId);
    } else {
        fetchCourseWorkFromApi(courseId);
    }
}

QJsonDocument ClassroomClient::loadSampleDocument() const
{
    QByteArray data;

    if (!m_sampleDataPath.trimmed().isEmpty()) {
        QFile file(m_sampleDataPath);
        if (file.open(QIODevice::ReadOnly)) {
            data = file.readAll();
            file.close();
        }
    }

    if (data.isEmpty()) {
        QFile resourceFile(QStringLiteral(":/data/sample_classroom_data.json"));
        if (resourceFile.open(QIODevice::ReadOnly)) {
            data = resourceFile.readAll();
            resourceFile.close();
        }
    }

    if (data.isEmpty()) {
        return {};
    }

    return QJsonDocument::fromJson(data);
}

void ClassroomClient::fetchCoursesFromSample()
{
    QTimer::singleShot(0, this, [this]() {
        const QJsonDocument doc = loadSampleDocument();
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("courses"), QStringLiteral("No se pudo leer sample_classroom_data.json"));
            return;
        }

        const QJsonArray coursesArray = doc.object().value(QStringLiteral("courses")).toArray();
        emit coursesFetched(parseCourses(coursesArray));
    });
}

void ClassroomClient::fetchCourseWorkFromSample(const QString &courseId)
{
    QTimer::singleShot(0, this, [this, courseId]() {
        const QJsonDocument doc = loadSampleDocument();
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("courseWork:") + courseId, QStringLiteral("No se pudo leer sample_classroom_data.json"));
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonObject courseworkByCourse = root.value(QStringLiteral("courseWork")).toObject();
        const QJsonArray assignments = courseworkByCourse.value(courseId).toArray();
        emit courseWorkFetched(courseId, parseCourseWork(courseId, assignments));
    });
}

void ClassroomClient::fetchCoursesFromApi()
{
    if (m_accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("courses"), QStringLiteral("No hay access token para llamar Classroom API."));
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://classroom.googleapis.com/v1/courses?courseStates=ACTIVE")));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    QNetworkReply *reply = m_networkManager.get(request);
    reply->setProperty("requestType", QStringLiteral("courses"));
    connect(reply, &QNetworkReply::finished, this, &ClassroomClient::onReplyFinished);
}

void ClassroomClient::fetchCourseWorkFromApi(const QString &courseId)
{
    if (m_accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("courseWork:") + courseId, QStringLiteral("No hay access token para llamar Classroom API."));
        return;
    }

    const QString encodedCourseId = QString::fromUtf8(QUrl::toPercentEncoding(courseId));
    const QUrl url(QStringLiteral("https://classroom.googleapis.com/v1/courses/%1/courseWork").arg(encodedCourseId));

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    QNetworkReply *reply = m_networkManager.get(request);
    reply->setProperty("requestType", QStringLiteral("courseWork"));
    reply->setProperty("courseId", courseId);
    connect(reply, &QNetworkReply::finished, this, &ClassroomClient::onReplyFinished);
}

void ClassroomClient::onReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }

    const QString requestType = reply->property("requestType").toString();
    const QString courseId = reply->property("courseId").toString();
    const QByteArray payload = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        const QString detail = QString::fromUtf8(payload);
        emit errorOccurred(
            requestType == QStringLiteral("courseWork") ? QStringLiteral("courseWork:") + courseId : QStringLiteral("courses"),
            detail.isEmpty() ? reply->errorString() : detail);
        reply->deleteLater();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        emit errorOccurred(
            requestType == QStringLiteral("courseWork") ? QStringLiteral("courseWork:") + courseId : QStringLiteral("courses"),
            QStringLiteral("Respuesta JSON invalida de Classroom API."));
        reply->deleteLater();
        return;
    }

    const QJsonObject root = doc.object();

    if (requestType == QStringLiteral("courses")) {
        emit coursesFetched(parseCourses(root.value(QStringLiteral("courses")).toArray()));
    } else if (requestType == QStringLiteral("courseWork")) {
        emit courseWorkFetched(courseId, parseCourseWork(courseId, root.value(QStringLiteral("courseWork")).toArray()));
    }

    reply->deleteLater();
}

QList<Course> ClassroomClient::parseCourses(const QJsonArray &array) const
{
    QList<Course> courses;
    courses.reserve(array.size());

    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        courses.append(parseCourseObject(value.toObject()));
    }

    return courses;
}

QList<Assignment> ClassroomClient::parseCourseWork(const QString &courseId, const QJsonArray &array) const
{
    QList<Assignment> assignments;
    assignments.reserve(array.size());

    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        assignments.append(parseAssignmentObject(courseId, value.toObject()));
    }

    return assignments;
}

Course ClassroomClient::parseCourseObject(const QJsonObject &json) const
{
    Course course;
    course.id = json.value(QStringLiteral("id")).toString();
    course.name = json.value(QStringLiteral("name")).toString();
    course.section = json.value(QStringLiteral("section")).toString();
    course.descriptionHeading = json.value(QStringLiteral("descriptionHeading")).toString();
    course.alternateLink = json.value(QStringLiteral("alternateLink")).toString();
    return course;
}

Assignment ClassroomClient::parseAssignmentObject(const QString &courseId, const QJsonObject &json) const
{
    Assignment assignment;
    assignment.id = json.value(QStringLiteral("id")).toString();
    assignment.courseId = courseId;
    assignment.title = json.value(QStringLiteral("title")).toString();
    assignment.description = json.value(QStringLiteral("description")).toString();
    assignment.workType = json.value(QStringLiteral("workType")).toString();
    assignment.state = json.value(QStringLiteral("state")).toString();
    assignment.alternateLink = json.value(QStringLiteral("alternateLink")).toString();

    const QJsonObject dueDateObj = json.value(QStringLiteral("dueDate")).toObject();
    if (!dueDateObj.isEmpty()) {
        assignment.dueDate = QDate(
            dueDateObj.value(QStringLiteral("year")).toInt(),
            dueDateObj.value(QStringLiteral("month")).toInt(),
            dueDateObj.value(QStringLiteral("day")).toInt());
    }

    const QJsonObject dueTimeObj = json.value(QStringLiteral("dueTime")).toObject();
    if (!dueTimeObj.isEmpty()) {
        assignment.dueTime = QTime(
            dueTimeObj.value(QStringLiteral("hours")).toInt(),
            dueTimeObj.value(QStringLiteral("minutes")).toInt(),
            dueTimeObj.value(QStringLiteral("seconds")).toInt(),
            dueTimeObj.value(QStringLiteral("nanos")).toInt() / 1000000);
    }

    assignment.rawJson = json;
    return assignment;
}
