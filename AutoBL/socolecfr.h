#ifndef SOCOLECFR_H
#define SOCOLECFR_H

#include "db.h"
#include "fctfournisseur.h"
#include <QDebug>
#include <QObject>

#define DEBUG qDebug()
#define FRN "Socolec.fr"
#define INF "Numéro de compte|Email|Mot de passe"

class SocolecFr: public QObject
{
    Q_OBJECT

public:
    SocolecFr(FctFournisseur *fct, const QString login, const QString mdp, const QString lien_Travail, const QString comp, DB *db);
    bool Start();
    QStringList Get_Invoice(const QString InvoiceNumber, QString link);
    void Set_Var(const QString login,const QString mdp,const QString comp);
    bool Test_Connexion();
    static QString Get_Inf();

private slots:
    bool Connexion();
    bool Create_List_Invoice();
    bool Update_State(QString invoice, QString link);
    bool Update_Delivery(QString invoice, QString link);

signals:
    void Info(const QString i);
    void FindTexte(const QString texte);

private:
    QString m_Login,m_MDP,m_UserName,m_WorkLink;
    FctFournisseur *m_Fct;
    DB *m_DB;
};

#endif // SOCOLECFR_H
