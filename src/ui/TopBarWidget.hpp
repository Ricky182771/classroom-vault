#pragma once

#include <QFrame>
#include <QString>
#include <QStringList>

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;

class TopBarWidget : public QFrame {
    Q_OBJECT

public:
    explicit TopBarWidget(QWidget *parent = nullptr);

    void setPageTitle(const QString &title);
    void setConnectionState(const QString &state);
    void setConnectedEmail(const QString &email);
    void setSearchPlaceholder(const QString &placeholder);
    // La lista de semestres se puebla desde la configuracion real (ver
    // MainWindow::knownSemesters), no cableada: un semestre con nombre propio
    // tiene que poder seleccionarse y archivarse.
    void setAvailableSemesters(const QStringList &semesters);
    void setGlobalSemesterFilter(const QString &semester);
    QString globalSemesterFilter() const;
    void setSemesterArchived(bool archived);
    // Destino de escritura de las materias nuevas. Es una decision del modelo de
    // datos y por eso tiene control propio: antes se derivaba del combo de filtro,
    // de modo que mirar un semestre reasignaba las materias sin mapeo explicito.
    void setTargetSemesterOptions(const QStringList &semesters, const QString &current);

signals:
    void syncRequested();
    void accountRequested();
    void searchRequested(const QString &text);
    void searchTextChanged(const QString &text);
    void globalSemesterFilterChanged(const QString &semester);
    void archiveSemesterRequested(const QString &semester);
    void targetSemesterChanged(const QString &semester);

private:
    static bool isArchivableSemester(const QString &semester);
    void updateArchiveControls();

    QLabel *m_titleLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_emailLabel = nullptr;
    QComboBox *m_semesterCombo = nullptr;
    QComboBox *m_targetSemesterCombo = nullptr;
    QPushButton *m_syncButton = nullptr;
    QPushButton *m_accountButton = nullptr;
    QPushButton *m_archiveButton = nullptr;
    QLabel *m_archivedLockLabel = nullptr;
    bool m_semesterArchived = false;
};
