#include "../Semester.hpp"
#include "TopBarWidget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>

TopBarWidget::TopBarWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("TopBar"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto *appLabel = new QLabel(QStringLiteral("Classroom Vault"), this);
    appLabel->setStyleSheet(QStringLiteral("font-size:15px;font-weight:700;background:transparent;border:none;"));
    layout->addWidget(appLabel);

    m_titleLabel = new QLabel(QStringLiteral("Inicio"), this);
    m_titleLabel->setProperty("subtle", true);
    layout->addWidget(m_titleLabel);

    layout->addSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Buscar materias, tareas o archivos…"));
    m_searchEdit->setMinimumWidth(260);
    layout->addWidget(m_searchEdit, 1);

    m_semesterCombo = new QComboBox(this);
    m_semesterCombo->setMinimumWidth(170);
    // Contenido inicial minimo: MainWindow lo repuebla con los semestres reales.
    m_semesterCombo->addItem(Semester::all());
    m_semesterCombo->addItem(Semester::none());
    layout->addWidget(m_semesterCombo);

    auto *targetLabel = new QLabel(QStringLiteral("Materias nuevas \u2192"), this);
    targetLabel->setProperty("subtle", true);
    targetLabel->setToolTip(QStringLiteral("Semestre en el que se guardaran las materias que aun no tienen uno asignado."));
    layout->addWidget(targetLabel);

    m_targetSemesterCombo = new QComboBox(this);
    m_targetSemesterCombo->setMinimumWidth(140);
    m_targetSemesterCombo->setToolTip(targetLabel->toolTip());
    layout->addWidget(m_targetSemesterCombo);

    m_archivedLockLabel = new QLabel(QStringLiteral("\U0001F512 Archivado"), this);
    m_archivedLockLabel->setToolTip(QStringLiteral("Semestre archivado: solo lectura"));
    m_archivedLockLabel->setStyleSheet(
        QStringLiteral("padding:3px 8px;border-radius:8px;font-size:11px;color:#E6C26A;"
                       "background:rgba(230,194,106,0.16);border:1px solid rgba(255,255,255,0.12);"));
    m_archivedLockLabel->setVisible(false);
    layout->addWidget(m_archivedLockLabel);

    m_archiveButton = new QPushButton(QStringLiteral("Archivar semestre"), this);
    m_archiveButton->setProperty("variant", QStringLiteral("ghost"));
    layout->addWidget(m_archiveButton);

    m_rescueButton = new QPushButton(QStringLiteral("Rescatar materias"), this);
    m_rescueButton->setProperty("variant", QStringLiteral("primary"));
    m_rescueButton->setVisible(false);
    layout->addWidget(m_rescueButton);

    auto *stateTag = new QFrame(this);
    stateTag->setObjectName(QStringLiteral("Section"));
    auto *stateLayout = new QHBoxLayout(stateTag);
    stateLayout->setContentsMargins(8, 5, 8, 5);
    stateLayout->setSpacing(6);

    auto *dot = new QLabel(QStringLiteral("●"), stateTag);
    dot->setStyleSheet(QStringLiteral("color:#8FD19E;font-size:11px;background:transparent;border:none;"));
    stateLayout->addWidget(dot);

    m_connectionLabel = new QLabel(QStringLiteral("Conectado"), stateTag);
    stateLayout->addWidget(m_connectionLabel);

    m_emailLabel = new QLabel(QStringLiteral("—"), stateTag);
    m_emailLabel->setProperty("subtle", true);
    stateLayout->addWidget(m_emailLabel);

    layout->addWidget(stateTag);

    m_syncButton = new QPushButton(QStringLiteral("Sincronizar"), this);
    m_syncButton->setProperty("variant", QStringLiteral("primary"));
    layout->addWidget(m_syncButton);

    m_accountButton = new QPushButton(QStringLiteral("Cuenta"), this);
    m_accountButton->setProperty("variant", QStringLiteral("ghost"));
    layout->addWidget(m_accountButton);

    auto *avatar = new QLabel(QStringLiteral("R"), this);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(30, 30);
    avatar->setObjectName(QStringLiteral("Section"));
    avatar->setStyleSheet(QStringLiteral("border-radius:15px;font-weight:700;background:#30323A;border:1px solid #3A3D46;"));
    layout->addWidget(avatar);

    connect(m_syncButton, &QPushButton::clicked, this, &TopBarWidget::syncRequested);
    connect(m_accountButton, &QPushButton::clicked, this, &TopBarWidget::accountRequested);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        emit searchRequested(m_searchEdit->text().trimmed());
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit searchTextChanged(text.trimmed());
    });
    connect(m_semesterCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        // Al cambiar de semestre el estado de archivado deja de ser conocido:
        // se recalcula la habilitacion y MainWindow reenvia el valor real.
        m_semesterArchived = false;
        updateArchiveControls();
        emit globalSemesterFilterChanged(text.trimmed());
    });
    connect(m_targetSemesterCombo, &QComboBox::activated, this, [this](int) {
        emit targetSemesterChanged(m_targetSemesterCombo->currentText().trimmed());
    });
    connect(m_rescueButton, &QPushButton::clicked, this, &TopBarWidget::releaseTrappedCoursesRequested);
    connect(m_archiveButton, &QPushButton::clicked, this, [this]() {
        const QString semester = m_semesterCombo->currentText().trimmed();
        if (!isArchivableSemester(semester)) {
            return;
        }
        if (m_semesterArchived) {
            emit unarchiveSemesterRequested(semester);
        } else {
            emit archiveSemesterRequested(semester);
        }
    });

    updateArchiveControls();
}

bool TopBarWidget::isArchivableSemester(const QString &semester)
{
    const QString clean = semester.trimmed();
    if (clean.isEmpty()) {
        return false;
    }
    // "Todos los semestres" y "Sin semestre" son centinelas, no semestres reales.
    return clean != Semester::all() && clean != Semester::none();
}

void TopBarWidget::updateArchiveControls()
{
    const QString semester = m_semesterCombo->currentText().trimmed();
    const bool archivable = isArchivableSemester(semester);

    m_archivedLockLabel->setVisible(archivable && m_semesterArchived);
    m_archiveButton->setEnabled(archivable);
    // El boton alterna: archivar era irreversible desde la app, asi que un mapeo
    // escrito por error dejaba el semestre congelado para siempre.
    m_archiveButton->setText(archivable && m_semesterArchived
                                 ? QStringLiteral("Desarchivar semestre")
                                 : QStringLiteral("Archivar semestre"));

    if (!archivable) {
        m_archiveButton->setToolTip(
            QStringLiteral("Selecciona un semestre concreto para poder archivarlo."));
    } else if (m_semesterArchived) {
        m_archiveButton->setToolTip(
            QStringLiteral("Desarchivar %1: vuelve a sincronizarse con Classroom.").arg(semester));
    } else {
        m_archiveButton->setToolTip(
            QStringLiteral("Archivar %1: lo desconecta de Classroom y lo deja en solo lectura.").arg(semester));
    }
}

void TopBarWidget::setSemesterArchived(bool archived)
{
    m_semesterArchived = archived;
    updateArchiveControls();
}

void TopBarWidget::setPageTitle(const QString &title)
{
    m_titleLabel->setText(title.trimmed().isEmpty() ? QStringLiteral("Inicio") : title.trimmed());
}

void TopBarWidget::setConnectionState(const QString &state)
{
    m_connectionLabel->setText(state);
}

void TopBarWidget::setConnectedEmail(const QString &email)
{
    m_emailLabel->setText(email.trimmed().isEmpty() ? QStringLiteral("—") : email.trimmed());
}

void TopBarWidget::setSearchPlaceholder(const QString &placeholder)
{
    m_searchEdit->setPlaceholderText(
        placeholder.trimmed().isEmpty() ? QStringLiteral("Buscar materias, tareas o archivos…") : placeholder.trimmed());
}

void TopBarWidget::setAvailableSemesters(const QStringList &semesters)
{
    QStringList items;
    items << Semester::all() << Semester::none();
    for (const QString &semester : semesters) {
        const QString clean = semester.trimmed();
        if (!clean.isEmpty() && !items.contains(clean)) {
            items.append(clean);
        }
    }

    // El valor seleccionado nunca se pierde en la repoblacion: si desapareciera de la
    // lista, el combo divergiria en silencio del filtro real de MainWindow.
    const QString previous = m_semesterCombo->currentText().trimmed();
    if (!previous.isEmpty() && !items.contains(previous)) {
        items.append(previous);
    }

    QStringList currentItems;
    currentItems.reserve(m_semesterCombo->count());
    for (int i = 0; i < m_semesterCombo->count(); ++i) {
        currentItems.append(m_semesterCombo->itemText(i));
    }
    if (currentItems == items) {
        return;
    }

    m_semesterCombo->blockSignals(true);
    m_semesterCombo->clear();
    m_semesterCombo->addItems(items);
    const int previousIndex = m_semesterCombo->findText(previous);
    m_semesterCombo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
    m_semesterCombo->blockSignals(false);

    updateArchiveControls();
}

void TopBarWidget::setGlobalSemesterFilter(const QString &semester)
{
    const QString clean = semester.trimmed().isEmpty() ? Semester::all() : semester.trimmed();
    int idx = m_semesterCombo->findText(clean);
    if (idx < 0) {
        // Antes se descartaba en silencio y el combo quedaba mostrando otro semestre.
        m_semesterCombo->blockSignals(true);
        m_semesterCombo->addItem(clean);
        m_semesterCombo->blockSignals(false);
        idx = m_semesterCombo->findText(clean);
    }
    if (idx < 0) {
        return;
    }

    if (m_semesterCombo->currentIndex() == idx) {
        return;
    }

    // Setter programatico: no puede confundirse con una interaccion del usuario,
    // o MainWindow reentra en onGlobalSemesterFilterChanged y persiste config.
    m_semesterCombo->blockSignals(true);
    m_semesterCombo->setCurrentIndex(idx);
    m_semesterCombo->blockSignals(false);
    updateArchiveControls();
}

void TopBarWidget::setTrappedCourseCount(int count, const QString &targetSemester)
{
    m_rescueButton->setVisible(count > 0);
    if (count <= 0) {
        return;
    }

    m_rescueButton->setText(QStringLiteral("Rescatar %1 materias").arg(count));
    m_rescueButton->setToolTip(
        QStringLiteral("%1 materias siguen activas en Classroom pero su semestre esta archivado, "
                       "asi que ningun sync las respalda. Moverlas a %2.")
            .arg(count)
            .arg(targetSemester.trimmed().isEmpty() ? QStringLiteral("el semestre destino") : targetSemester));
}

void TopBarWidget::setTargetSemesterOptions(const QStringList &semesters, const QString &current)
{
    const QString clean = current.trimmed();
    QStringList items = semesters;
    if (!clean.isEmpty() && !items.contains(clean)) {
        items.prepend(clean);
    }

    m_targetSemesterCombo->blockSignals(true);
    m_targetSemesterCombo->clear();
    m_targetSemesterCombo->addItems(items);
    const int idx = m_targetSemesterCombo->findText(clean);
    m_targetSemesterCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_targetSemesterCombo->blockSignals(false);
}

QString TopBarWidget::globalSemesterFilter() const
{
    return m_semesterCombo->currentText().trimmed();
}
