#ifndef REXELFR_H
#define REXELFR_H

#include <QObject>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>
#include <QSqlQuery>

#include "db.h"
#include "fctfournisseur.h"

#define DEBUG qDebug()

class RexelFr: public QObject
{
    Q_OBJECT

public:
    RexelFr(FctFournisseur *fct, const QString login, const QString mdp, const QString lien_Travail, const QString comp);
    bool Start();
    QStringList Get_Invoice(const QString InvoiceNumber);
    void Set_Var(const QString login,const QString mdp,const QString comp);
    bool Test_Connexion();

private slots:
    bool Connexion();
    bool Create_List_Invoice();
    bool Check_Delivery(const QString InvoiceNumber);
    bool Check_State(const QString InvoiceNumber);


signals:
    void Info(const QString i);
    //void Error(const QString error,int code);
    void FindTexte(const QString texte);

private:
    QString m_Login,m_MDP,m_UserName,m_WorkLink;
    FctFournisseur *m_Fct;
    DB m_DB;

};

#endif // REXELFR_H
