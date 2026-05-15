#include "ClassroomClient.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace {

QString extractErrorMessage(const QByteArray &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return QString::fromUtf8(payload).trimmed();
    }

    const QJsonObject root = doc.object();
    const QJsonObject errorObj = root.value(QStringLiteral("error")).toObject();
    const QString apiMessage = errorObj.value(QStringLiteral("message")).toString().trimmed();
    if (!apiMessage.isEmpty()) {
        return apiMessage;
    }

    return QString::fromUtf8(payload).trimmed();
}

int submissionStatePriority(const QString &submissionState)
{
    const QString state = submissionState.trimmed().toUpper();
    if (state == QStringLiteral("RETURNED")) {
        return 4;
    }
    if (state == QStringLiteral("TURNED_IN")) {
        return 3;
    }
    if (state == QStringLiteral("RECLAIMED_BY_STUDENT")) {
        return 2;
    }
    if (state == QStringLiteral("CREATED")) {
        return 1;
    }
    if (state == QStringLiteral("NEW")) {
        return 0;
    }
    return -1;
}

bool isSubmissionStateReliable(const QString &submissionState)
{
    const QString state = submissionState.trimmed().toUpper();
    return state == QStringLiteral("NEW")
        || state == QStringLiteral("CREATED")
        || state == QStringLiteral("TURNED_IN")
        || state == QStringLiteral("RETURNED")
        || state == QStringLiteral("RECLAIMED_BY_STUDENT");
}

} // namespace

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
            emit requestFailed(QStringLiteral("courses"), 0, QStringLiteral("No se pudo leer sample_classroom_data.json"));
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
            emit requestFailed(QStringLiteral("courseWork:") + courseId, 0, QStringLiteral("No se pudo leer sample_classroom_data.json"));
            emit errorOccurred(QStringLiteral("courseWork:") + courseId, QStringLiteral("No se pudo leer sample_classroom_data.json"));
            return;
        }

        const QJsonObject root = doc.object();
        const QJsonObject courseworkByCourse = root.value(QStringLiteral("courseWork")).toObject();
        const QJsonArray assignments = courseworkByCourse.value(courseId).toArray();
        m_courseWorkAccumulator.insert(courseId, parseCourseWork(courseId, assignments));

        const QJsonObject submissionsByCourse = root.value(QStringLiteral("studentSubmissions")).toObject();
        const QJsonArray submissionsArray = submissionsByCourse.value(courseId).toArray();
        QList<QJsonObject> submissions;
        submissions.reserve(submissionsArray.size());
        for (const QJsonValue &value : submissionsArray) {
            if (value.isObject()) {
                submissions.append(value.toObject());
            }
        }

        const bool reliable = submissionsByCourse.contains(courseId);
        applyStudentSubmissionsToAssignments(courseId, submissions, reliable);
        finalizeCourseWorkSnapshot(courseId, true, reliable);
    });
}

void ClassroomClient::fetchCoursesFromApi()
{
    if (m_accessToken.isEmpty()) {
        emit requestFailed(QStringLiteral("courses"), 0, QStringLiteral("No hay access token para llamar Classroom API."));
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
        emit requestFailed(QStringLiteral("courseWork:") + courseId, 0, QStringLiteral("No hay access token para llamar Classroom API."));
        emit errorOccurred(QStringLiteral("courseWork:") + courseId, QStringLiteral("No hay access token para llamar Classroom API."));
        return;
    }

    m_courseWorkAccumulator.remove(courseId);
    m_courseWorkPageCount.insert(courseId, 0);
    m_studentSubmissionsAccumulator.remove(courseId);
    m_studentSubmissionsPageCount.insert(courseId, 0);
    fetchCourseWorkPageFromApi(courseId, QString());
}

void ClassroomClient::fetchCourseWorkPageFromApi(const QString &courseId, const QString &pageToken)
{
    const QString encodedCourseId = QString::fromUtf8(QUrl::toPercentEncoding(courseId));
    QUrl url(QStringLiteral("https://classroom.googleapis.com/v1/courses/%1/courseWork").arg(encodedCourseId));
    if (!pageToken.trimmed().isEmpty()) {
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("pageToken"), pageToken.trimmed());
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    QNetworkReply *reply = m_networkManager.get(request);
    reply->setProperty("requestType", QStringLiteral("courseWork"));
    reply->setProperty("courseId", courseId);
    reply->setProperty("pageToken", pageToken);
    connect(reply, &QNetworkReply::finished, this, &ClassroomClient::onReplyFinished);
}

void ClassroomClient::fetchStudentSubmissionsFromApi(const QString &courseId)
{
    m_studentSubmissionsAccumulator.remove(courseId);
    m_studentSubmissionsPageCount.insert(courseId, 0);
    fetchStudentSubmissionsPageFromApi(courseId, QString());
}

void ClassroomClient::fetchStudentSubmissionsPageFromApi(const QString &courseId, const QString &pageToken)
{
    const QString encodedCourseId = QString::fromUtf8(QUrl::toPercentEncoding(courseId));
    QUrl url(QStringLiteral("https://classroom.googleapis.com/v1/courses/%1/courseWork/-/studentSubmissions").arg(encodedCourseId));
    if (!pageToken.trimmed().isEmpty()) {
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("pageToken"), pageToken.trimmed());
        url.setQuery(query);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

    QNetworkReply *reply = m_networkManager.get(request);
    reply->setProperty("requestType", QStringLiteral("studentSubmissions"));
    reply->setProperty("courseId", courseId);
    reply->setProperty("pageToken", pageToken);
    connect(reply, &QNetworkReply::finished, this, &ClassroomClient::onReplyFinished);
}

void ClassroomClient::applyStudentSubmissionsToAssignments(
    const QString &courseId,
    const QList<QJsonObject> &submissionObjects,
    bool reliable)
{
    QList<Assignment> assignments = m_courseWorkAccumulator.value(courseId);
    if (assignments.isEmpty()) {
        return;
    }

    QHash<QString, QJsonObject> bestSubmissionByCourseWorkId;
    for (const QJsonObject &submission : submissionObjects) {
        const QString courseWorkId = submission.value(QStringLiteral("courseWorkId")).toString().trimmed();
        if (courseWorkId.isEmpty()) {
            continue;
        }

        const QJsonObject currentBest = bestSubmissionByCourseWorkId.value(courseWorkId);
        if (currentBest.isEmpty()) {
            bestSubmissionByCourseWorkId.insert(courseWorkId, submission);
            continue;
        }

        const QString stateCandidate = submission.value(QStringLiteral("state")).toString();
        const QString stateCurrent = currentBest.value(QStringLiteral("state")).toString();
        const int priorityCandidate = submissionStatePriority(stateCandidate);
        const int priorityCurrent = submissionStatePriority(stateCurrent);

        if (priorityCandidate > priorityCurrent) {
            bestSubmissionByCourseWorkId.insert(courseWorkId, submission);
            continue;
        }
        if (priorityCandidate < priorityCurrent) {
            continue;
        }

        const QString updateCandidate = submission.value(QStringLiteral("updateTime")).toString().trimmed();
        const QString updateCurrent = currentBest.value(QStringLiteral("updateTime")).toString().trimmed();
        if (updateCandidate > updateCurrent) {
            bestSubmissionByCourseWorkId.insert(courseWorkId, submission);
        }
    }

    for (Assignment &assignment : assignments) {
        assignment.submissionId.clear();
        assignment.submissionState.clear();
        assignment.submissionUpdateTime.clear();
        assignment.submissionAlternateLink.clear();
        assignment.submissionLate = false;
        assignment.submissionStateReliable = false;

        const QJsonObject submission = bestSubmissionByCourseWorkId.value(assignment.id);
        if (submission.isEmpty()) {
            continue;
        }

        assignment.submissionId = submission.value(QStringLiteral("id")).toString().trimmed();
        assignment.submissionState = submission.value(QStringLiteral("state")).toString().trimmed();
        assignment.submissionUpdateTime = submission.value(QStringLiteral("updateTime")).toString().trimmed();
        assignment.submissionAlternateLink = submission.value(QStringLiteral("alternateLink")).toString().trimmed();
        assignment.submissionLate = submission.value(QStringLiteral("late")).toBool(false);
        assignment.submissionStateReliable = reliable && isSubmissionStateReliable(assignment.submissionState);
    }

    m_courseWorkAccumulator.insert(courseId, assignments);
}

void ClassroomClient::finalizeCourseWorkSnapshot(const QString &courseId, bool fetchComplete, bool submissionsReliable)
{
    QList<Assignment> assignments = m_courseWorkAccumulator.value(courseId);
    if (!submissionsReliable) {
        for (Assignment &assignment : assignments) {
            assignment.submissionStateReliable = false;
        }
    }

    emit courseWorkFetched(courseId, assignments);
    emit courseWorkSnapshotFetched(courseId, assignments, fetchComplete);

    m_courseWorkAccumulator.remove(courseId);
    m_courseWorkPageCount.remove(courseId);
    m_studentSubmissionsAccumulator.remove(courseId);
    m_studentSubmissionsPageCount.remove(courseId);
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
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString context = requestType == QStringLiteral("courseWork")
        ? QStringLiteral("courseWork:") + courseId
        : (requestType == QStringLiteral("studentSubmissions")
               ? QStringLiteral("studentSubmissions:") + courseId
               : QStringLiteral("courses"));

    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
        QString message = extractErrorMessage(payload);
        if (message.isEmpty()) {
            message = reply->errorString();
        }

        if (httpStatus == 401) {
            message = QStringLiteral("Token invalido o expirado (401). ") + message;
            emit tokenInvalid();
        } else if (httpStatus == 403) {
            message = QStringLiteral("Permiso insuficiente (403). ") + message;
        } else if (httpStatus == 404) {
            message = QStringLiteral("Recurso no encontrado (404). ") + message;
        } else if (httpStatus == 0 && reply->error() != QNetworkReply::NoError) {
            message = QStringLiteral("Error de red. ") + message;
        }

        if (requestType == QStringLiteral("courseWork")) {
            emit requestFailed(context, httpStatus, message);
            emit errorOccurred(context, message);
            const QList<Assignment> partial = m_courseWorkAccumulator.value(courseId);
            emit courseWorkSnapshotFetched(courseId, partial, false);
            m_courseWorkAccumulator.remove(courseId);
            m_courseWorkPageCount.remove(courseId);
            m_studentSubmissionsAccumulator.remove(courseId);
            m_studentSubmissionsPageCount.remove(courseId);
        } else if (requestType == QStringLiteral("studentSubmissions")) {
            if (httpStatus == 401) {
                emit logMessage(QStringLiteral("WARN  Token invalido/expirado al leer submissions (%1). Se omitira estado de entrega en esta sincronizacion.").arg(courseId));
            } else if (httpStatus == 403) {
                emit logMessage(QStringLiteral("WARN  No se pudo leer submissions (%1). Verifica scope classroom.student-submissions.me.readonly y reautoriza token.").arg(courseId));
            } else {
                emit logMessage(QStringLiteral("WARN  No se pudo leer submissions (%1). Se evitara marcar 'No entregada' sin evidencia.").arg(courseId));
            }
            finalizeCourseWorkSnapshot(courseId, true, false);
        } else {
            emit requestFailed(context, httpStatus, message);
            emit errorOccurred(context, message);
        }

        reply->deleteLater();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        if (requestType == QStringLiteral("studentSubmissions")) {
            emit logMessage(QStringLiteral("WARN  Respuesta JSON invalida en submissions (%1). Se omitira estado de entrega.").arg(courseId));
            finalizeCourseWorkSnapshot(courseId, true, false);
        } else {
            emit requestFailed(context, httpStatus, QStringLiteral("Respuesta JSON invalida de Classroom API."));
            emit errorOccurred(context, QStringLiteral("Respuesta JSON invalida de Classroom API."));
        }
        reply->deleteLater();
        return;
    }

    const QJsonObject root = doc.object();

    if (requestType == QStringLiteral("courses")) {
        emit coursesFetched(parseCourses(root.value(QStringLiteral("courses")).toArray()));
    } else if (requestType == QStringLiteral("courseWork")) {
        const QList<Assignment> pageAssignments = parseCourseWork(courseId, root.value(QStringLiteral("courseWork")).toArray());

        QList<Assignment> accumulated = m_courseWorkAccumulator.value(courseId);
        accumulated.append(pageAssignments);
        m_courseWorkAccumulator.insert(courseId, accumulated);

        const int pageNo = m_courseWorkPageCount.value(courseId, 0) + 1;
        m_courseWorkPageCount.insert(courseId, pageNo);
        emit logMessage(QStringLiteral("API   Página %1 recibida (%2 tareas): %3").arg(pageNo).arg(pageAssignments.size()).arg(courseId));

        const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString().trimmed();
        if (!nextPageToken.isEmpty()) {
            reply->deleteLater();
            fetchCourseWorkPageFromApi(courseId, nextPageToken);
            return;
        }

        fetchStudentSubmissionsFromApi(courseId);
    } else if (requestType == QStringLiteral("studentSubmissions")) {
        const QList<QJsonObject> pageSubmissions = parseStudentSubmissions(root.value(QStringLiteral("studentSubmissions")).toArray());
        QList<QJsonObject> accumulated = m_studentSubmissionsAccumulator.value(courseId);
        accumulated.append(pageSubmissions);
        m_studentSubmissionsAccumulator.insert(courseId, accumulated);

        const int pageNo = m_studentSubmissionsPageCount.value(courseId, 0) + 1;
        m_studentSubmissionsPageCount.insert(courseId, pageNo);

        const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString().trimmed();
        if (!nextPageToken.isEmpty()) {
            reply->deleteLater();
            fetchStudentSubmissionsPageFromApi(courseId, nextPageToken);
            return;
        }

        const QList<QJsonObject> submissions = m_studentSubmissionsAccumulator.value(courseId);
        applyStudentSubmissionsToAssignments(courseId, submissions, true);
        emit logMessage(QStringLiteral("API   Submissions recibidas: %1 (%2)").arg(submissions.size()).arg(courseId));
        finalizeCourseWorkSnapshot(courseId, true, true);
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

QList<QJsonObject> ClassroomClient::parseStudentSubmissions(const QJsonArray &array) const
{
    QList<QJsonObject> submissions;
    submissions.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            submissions.append(value.toObject());
        }
    }
    return submissions;
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
    assignment.materials = parseMaterials(json.value(QStringLiteral("materials")).toArray());
    return assignment;
}

QVector<AssignmentMaterial> ClassroomClient::parseMaterials(const QJsonArray &array) const
{
    QVector<AssignmentMaterial> materials;
    materials.reserve(array.size());

    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        materials.append(parseMaterialObject(value.toObject()));
    }

    return materials;
}

AssignmentMaterial ClassroomClient::parseMaterialObject(const QJsonObject &json) const
{
    AssignmentMaterial material;
    material.rawJson = json;

    if (json.contains(QStringLiteral("driveFile")) && json.value(QStringLiteral("driveFile")).isObject()) {
        material.type = QStringLiteral("driveFile");
        const QJsonObject driveContainer = json.value(QStringLiteral("driveFile")).toObject();
        QJsonObject driveFile = driveContainer.value(QStringLiteral("driveFile")).toObject();
        if (driveFile.isEmpty()) {
            driveFile = driveContainer;
        }
        material.title = driveFile.value(QStringLiteral("title")).toString();
        material.alternateLink = driveFile.value(QStringLiteral("alternateLink")).toString();
        material.driveFileId = driveFile.value(QStringLiteral("id")).toString();
    } else if (json.contains(QStringLiteral("link")) && json.value(QStringLiteral("link")).isObject()) {
        material.type = QStringLiteral("link");
        const QJsonObject link = json.value(QStringLiteral("link")).toObject();
        material.title = link.value(QStringLiteral("title")).toString();
        material.url = link.value(QStringLiteral("url")).toString();
    } else if (json.contains(QStringLiteral("youtubeVideo")) && json.value(QStringLiteral("youtubeVideo")).isObject()) {
        material.type = QStringLiteral("youtubeVideo");
        const QJsonObject yt = json.value(QStringLiteral("youtubeVideo")).toObject();
        material.title = yt.value(QStringLiteral("title")).toString();
        material.alternateLink = yt.value(QStringLiteral("alternateLink")).toString();
        material.url = yt.value(QStringLiteral("alternateLink")).toString();
    } else if (json.contains(QStringLiteral("form")) && json.value(QStringLiteral("form")).isObject()) {
        material.type = QStringLiteral("form");
        const QJsonObject form = json.value(QStringLiteral("form")).toObject();
        material.title = form.value(QStringLiteral("title")).toString();
        material.url = form.value(QStringLiteral("formUrl")).toString();
    } else {
        material.type = QStringLiteral("unknown");
    }

    return material;
}
