#include "esabora.h"
#include <Windows.h>


Esabora::Esabora(QWidget *fen, QString Login, QString MDP, QString Lien_Esabora, QString Lien_Travail, DB *db,Error *e):
    m_fen(fen),m_Login(Login),m_MDP(MDP),m_Lien_Esabora(Lien_Esabora),m_Lien_Work(Lien_Travail),m_DB(db),err(e),etat(0)
{
    qDebug() << "Init Class Esabora";
    m_Arret = false;
}

Esabora::~Esabora()
{
    Clavier("Purge");
}

bool Esabora::Start(bool automatic,int &nbBC,int &nbBL)
{
    m_Arret = false;
    m_List_Cmd.clear();

    if(Lancement_API() == false) { return false; }

    ///Ajout des BC sur esabora
    QSqlQuery req = m_DB->Get_Download_Invoice();
    Ouverture_Liste_BC();
    qDebug() << "Liste BC ouverte";
    while(req.next() && m_Arret == false)
    {
        qDebug() << "Ajout BC";
        emit Info(tr("Ajout du bon de commande %0").arg(req.value("Numero_Commande").toString()));
        if(Get_List_Matos(req.value("Numero_Commande").toString()))
        {
            if(req.value("Nom_Chantier").toString() == "0")//Ajout BC au Stock
            {
                if(Ajout_Stock(req.value("Numero_Commande").toString()) == false)
                {
                    Abort();
                    Ouverture_Liste_BC();
                    err->Err(Not_BC,req.value("Numero_Commande").toString(),ESAB);
                }
                else
                {
                    m_DB->Requete("UPDATE En_Cours SET Ajout='" + QString::number(add) + "' WHERE Numero_Commande='" + req.value("Numero_Commande").toString() + "'");
                    nbBC++;
                }
            }      
            else//Ajout BC sur numéro de chantier
            {
                if(Ajout_BC(req.value("Numero_Commande").toString()) == false)
                {
                    Abort();
                    Ouverture_Liste_BC();
                    if(GetEtat() == 0)
                    {
                        err->Err(Not_BC,req.value("Numero_Commande").toString(),ESAB);
                    }
                    else if(GetEtat() == 1)
                    {
                        err->Err(BC,req.value("Numero_Commande").toString(),ESAB);
                        m_DB->Requete("UPDATE En_Cours SET Ajout='"+QString::number(error)+"' WHERE Numero_Commande='" + req.value("Numero_Commande").toString() + "'");
                    }
                }
                else
                {
                    m_DB->Requete("UPDATE En_Cours SET Ajout='" + QString::number(add) + "' WHERE Numero_Commande='" + req.value("Numero_Commande").toString() + "'");
                    nbBC++;
                    if(automatic)
                    {
                        m_List_Cmd.append(req.value("Numero_Commande").toString());
                    }
                }
            }
        }
        liste_Matos.clear();
    }
    Abort();


    ///Ajout des BL sur esabora
    req = m_DB->Get_Added_Invoice();
    while(req.next())
    {
        m_List_Cmd.append(req.value("Numero_Commande").toString());
    }

    for(int cpt=0;cpt<m_List_Cmd.count();cpt++)
    {
        if(m_Arret)
        {
            cpt = m_List_Cmd.count();
        }
        QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + m_List_Cmd.at(cpt) + "'");
        if(req.next())
        {
            if(Ajout_BL(req.value("Numero_BC_Esabora").toString(),req.value("Numero_Livraison").toString()) == false)
            {
                Abort();
                err->Err(BL,req.value("Numero_Livraison").toString(),ESAB);
            }
            else
            {
                nbBL++;
                emit Info(tr("Principal | Ajout BL N°%0 Réussi").arg(req.value("Numero_Livraison").toString()));
                m_DB->Requete("UPDATE En_Cours SET Ajout='"+QString::number(endAdd)+"' WHERE Numero_Commande='" + m_List_Cmd.at(cpt) + "'");
            }
        }
        else
        {
            err->Err(requete,m_List_Cmd.at(cpt),ESAB);
        }
    }

    return Fermeture_API();
}

///Erreur 2xx
bool Esabora::Lancement_API()
{
    qDebug() << "Esabora::Lancement_API()";

    QSqlQuery r = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='12'");
    r.next();
    QString db = r.value("Valeur").toString();
    r = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='23'");
    r.next();
    QString ent = r.value("Valeur").toString();

    p.setProgram(m_Lien_Esabora);
    if(m_Lien_Esabora.contains("Simul_Esabora"))
    {
        p.start();
    }
    else if(Verification_Fenetre(ent + " - SESSION : 1 - REPERTOIRE : " + db))
    {
        if(Verification_Focus(ent + " - SESSION : 1 - REPERTOIRE : " + db,true))
        {
            DEBUG << "Esabora Ouvert et au premier plan";
            Abort();
            return true;
        }
        else
        {
            DEBUG << "Esabora déjà ouvert";
            return false;
        }
    }
    else if(QDesktopServices::openUrl(m_Lien_Esabora) == false)
    {
        err->Err(Run_Esabora,"",ESAB);
        qDebug() << "Lancement_API - Echec ouverture d'Esabora.elec";
        qDebug() << "Fin Esabora::Lancement_API()";
        return false;
    }

    if(Traitement_Fichier_Config("Open") == false)
    {
        err->Err(open_File,"Config.esab",ESAB);
        qDebug() << "Lancement_API - Echec du traitement du login sur Esabora.elec";
        qDebug() << "Fin Esabora::Lancement_API()";
        return false;
    }
    qDebug() << "Fin Esabora::Lancement_API()";
    return true;

}
///Erreur 3xx

bool Esabora::Ouverture_Liste_BC()
{
    qDebug() << "Esabora::Ouverture_Liste_BC()";
    if(Traitement_Fichier_Config("Liste_BC")) { return true; }
    return false;
}

QString Esabora::Find_Fabricant(QString Fab)
{
    QClipboard *pp = QApplication::clipboard();
    pp->clear();
    DEBUG << "Contrôle/Ouverture Catalogue";
    if(Lancement_API() == false) { return QString(); }
    if(Verification_Fenetre("Recherche Produits") == false)
    {
        Abort();
        if(Traitement_Fichier_Config("Open_Cat") == false)
        {
            err->Err(Traitement,ESAB,"Find_Fabricant");
            return QString();
        }
    }
    DEBUG << "Recherche du Fabricant " << Fab;
    if(Traitement_Fichier_Config("Cat",Fab) == false)
    {
        err->Err(Traitement,ESAB,"Find_Fabricant");
        return QString();
    }
    DEBUG << "Test si Fabricant trouvé";
    QString var;
    if(pp->text().isEmpty() || pp->text().contains("(") == false)
    {
        DEBUG << "Constructeur " + Fab + " non trouvé sur Esabora";
    }
    else if(pp->text().contains("Vous n'avez pas les droits pour accéder à cette option !"))
    {
        DEBUG << "Le catalogue produits n'est pas accessible !";
    }

    else
    {
        var = pp->text().split("(").at(1);
        var = var.split(")").at(0);
    }
    return var;

}

bool Esabora::Ajout_BC(QString Numero_Commande)
{
    qDebug() << "Esabora::Ajout_BC()";
    QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + Numero_Commande + "'");
    req.next();
    QString bL;
    bL = m_Lien_Work + "/Pj/" + Numero_Commande + ".xlsx";

    if(Traitement_Fichier_Config("New_BC",bL))
    {
        qDebug() << "Ajout_BC - Traitement d'ajout de BC terminé";
        qDebug() << "Fin Esabora::Ajout_BC()";
        return true;
    }
    else
    {
        qDebug() << "Ajout_BC - Echec Traitement d'ajout de BC";
        qDebug() << "Fin Esabora::Ajout_BC()";
        return false;
    }
}
///Erreur 4xx
bool Esabora::Ajout_BL(QString Numero_Commande_Esab, QString Numero_BL)
{
    qDebug() << "Esabora::Ajout_BL()";
    if(Traitement_Fichier_Config("Valid_BL",Numero_Commande_Esab + " " + Numero_BL) == false)
    {
        err->Err(BL,Numero_Commande_Esab,ESAB);
        qDebug() << "Ajout_BL - Echec Traitement Valid_BL";
        qDebug() << "Fin Esabora::Ajout_BL()";
        return false;
    }
    qDebug() << "Ajout_BL - Traitement Valid_BL terminé";
    qDebug() << "Fin Esabora::Ajout_BL()";
    return true;
}
///Erreur 5xx
bool Esabora::Traitement_Fichier_Config(const QString file, const QString bL)
{
    qDebug() << "Esabora::Traitement_Fichier_Config()";
    etat = 0;
    QFile fichier(m_Lien_Work + "/Config/Config.esab");
    emit Info("Ouverture fichier Config.esab");
    if(fichier.exists() == false)
    {
        err->Err(open_File,fichier.fileName(),ESAB);
        qDebug() << "Traitement_Fichier_Config - Echec le fichier " << fichier.fileName() << "n'existe pas";
        return false;
    }
    if(fichier.open(QIODevice::ReadOnly) == false)
    {
        err->Err(open_File,fichier.fileName(),ESAB);
        qDebug() << "Traitement_Fichier_Config - Echec d'ouverture du fichier " << fichier.fileName();
        return false;
    }

    QTextStream flux(&fichier);

    QString temp;
    QEventLoop loop;
    QTimer tmp;
    connect(&tmp,SIGNAL(timeout()),&loop,SLOT(quit()));
    QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + bL.split("/").last().split(".").at(0) + "'");
    req.next();
    QSqlQuery r = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='14'");
    r.next();
    int varTmp = r.value("Valeur").toDouble() * 1000;
    qDebug() << "Esabora::Traitement_Fichier_Config - Recherche de " << file << "dans le fichier Config";
    while(flux.atEnd() == false && flux.readLine().contains("[" + file + "]") == false) {}

    if(flux.atEnd()) { return false; }
    qDebug() << "Esabora::Traitement_Fichier_Config - " << file << "trouvé dans le fichier";
    DEBUG << "Var bL = " << bL;
    while(flux.atEnd() == false)
    {
        tmp.start(varTmp);
        loop.exec();
        tmp.stop();

        temp = flux.readLine();
        qDebug() << "Traitement_Fichier_Config - Commande " << temp;
        emit Info("Préparation de la commande " + temp);

        if(temp.contains("/*"))
        {
            while(flux.readLine().contains("*/") == false){}
            temp = flux.readLine();
        }
        else if(temp == "{LOGIN}") { Clavier("-" + m_Login); }
        else if(temp == "{MDP}") { Clavier("-" + m_MDP); }
        else if(temp == "{CHANTIER}")
        {///Si le chantier est trouvé, sinon si la variable contient 0 ajout au stock
            if(req.value("Nom_Chantier").toString() != "" && req.value("Nom_Chantier").toString() != "0") { Clavier("-" + req.value("Nom_Chantier").toString()); }
            else if(req.value("Nom_Chantier").toString() == "0") { Clavier("-Stock"); }
        }
        else if(temp == "{INTERLOCUTEUR}") { Clavier("-Autobl"); }
        else if(temp == "{BC}") { Clavier("-" + bL); }
        else if(temp == "{NUMERO_COMMANDE_ESABORA}") { Clavier("-" + bL.split(" ").at(0)); }
        else if(temp == "{BL}") { Clavier("-" + bL.split(" ").at(1)); }
        else if(temp == "{COMMENTAIRE}")
        {
            QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_BC_Esabora='" + bL.split(" ").at(0) + "'");
            req.next();
            Clavier("-AutoBL : " + req.value("Fournisseur").toString() + " : BC " + req.value("Numero_Commande").toString());
        }
        else if(temp == "{FOURNISSEUR}")
        {
            QString v;
            QSqlQuery t = m_DB->Requete("SELECT * FROM Options WHERE Nom='" + req.value("Fournisseur").toString() + "Rcc'");
            t.next();
            Clavier("-" + t.value("Valeur").toString());
        }
        else if(temp == "{BOUCLE}")
        {
            //boucle de 6 strings designation,reference,fabricant,prix unitaire,quantité livré,quantité restante
            if(liste_Matos.isEmpty())
            {
                Abort();
                Ouverture_Liste_BC();
                return false;
            }
            QClipboard *pp = QApplication::clipboard();
            for(int cpt=0;cpt<liste_Matos.count();cpt++)
            {
                if(liste_Matos.count()-cpt < 7)
                {
                    err->Err(variable,"Ligne incomplète : " + liste_Matos.at(cpt),ESAB);
                }
                QEventLoop lp;
                QTimer t;
                connect(&t,SIGNAL(timeout()),&lp,SLOT(quit()));
                pp->clear();
                t.start(1000);
                lp.exec();
                Clavier("Ctrl+C");
                t.start(1000);
                lp.exec();
                emit Info(pp->text());
                if(pp->text() != "")//Si D3E, passé à la ligne suivante
                {
                    Clavier("Tab");
                    Clavier("Tab");
                    Clavier("Tab");
                    Clavier("Tab");
                    Clavier("Tab");
                    Clavier("Tab");
                }
                if(liste_Matos.at(cpt+1) == "")//Si Référence est vide
                {
                    err->Err(designation,"Ref=" + liste_Matos.at(cpt) + " Chantier=" + req.value("Nom_Chantier").toString(),ESAB);
                }
                Clavier("-" + liste_Matos.at(cpt+1));//Ref
                Clavier("Tab");
                pp->clear();
                t.start(1000);
                lp.exec();
                Clavier("Ctrl+C");
                t.start(1000);
                lp.exec();
                if(pp->text() == "" || pp->text() == liste_Matos.at(cpt+1) || liste_Matos.at(cpt+1).isEmpty() || pp->text() == " ")//Si La désignation n'a pas été trouvé
                {
                    err->Err(designation,"Ref=" + liste_Matos.at(cpt+1) + " Chantier=" + req.value("Nom_Chantier").toString(),ESAB);
                    pp->clear();
                    t.start(1000);
                    lp.exec();
                    pp->setText(liste_Matos.at(cpt));
                    t.start(1000);
                    lp.exec();
                    Clavier("Ctrl+V");
                }
                Clavier("Tab");
                Clavier("-" + liste_Matos.at(cpt+5));//Quantité
                Clavier("Tab");
                QString var = liste_Matos.at(cpt+4);
                var.replace(",",".");
                if(var.toDouble() > 0)
                {
                    Clavier("-" + liste_Matos.at(cpt+4));//Prix
                }
                Clavier("Tab");
                Clavier("Tab");
                Clavier("Tab");
                cpt += 6;
            }
        }
        else if(temp == "{BOUCLE_CONSTRUCTEUR}")
        {
            Clavier("-" + bL.toUpper());
            tmp.start(500);
            loop.exec();
            Clavier("Ctrl+C");
            tmp.start(1000);
            loop.exec();
            Clavier("Ctrl+A");
        }
        else if(temp[0] == '=')
        {
            QString t;
            if(bL.split("/").count() > 1)
            {
                QSqlQuery r = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + bL.split("/").last().split(".").at(0) + "'");
                if(r.next() == false)
                {
                    err->Err(requete,r.lastError().text(),ESAB);
                }

                t = r.value("Numero_BC_Esabora").toString();
                temp.replace("%0",t);
            }
            r = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='12'");
            r.next();
            temp.replace("%1",r.value("Valeur").toString());
            r = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='23'");
            r.next();
            temp.replace("%2",r.value("Valeur").toString());
            qDebug() << temp;

            QString fen = temp.split("=").at(1);
            fen.replace("Focus","");
            if(fen.at(fen.count()-1) == ' ')
            {
                fen.remove(fen.count()-1,fen.count()-1);
            }
            emit Info(fen);
            if(Verification_Fenetre(fen) == false)
            {
                err->Err(Window,fen,ESAB);
                return false;
            }
            else
            {
                bool t = false;
                if(temp.split(" ").last() == "Focus")
                {
                    t = true;
                }
                if(Verification_Focus(fen,t) == false)
                {
                    err->Err(Focus,fen,ESAB);
                    Clavier("Entrée");
                    Abort();
                    if(file == "New_BC")
                    {
                        Ouverture_Liste_BC();
                    }
                    return false;
                }
                emit Info("Esabora | Verification de fenêtre Réussis");
            }
        }
        else if(temp.split(" ").at(0) == "Pause" && temp.split(" ").count() == 2)
        {
            tmp.start(temp.split(" ").at(1).toInt() * 1000);
            loop.exec();
            tmp.stop();
            m_Tmp = "";
        }
        else if(temp.split(" ").at(0) == "Souris")
        {
            if(Souris(temp) == false)
            {
               err->Err(Mouse,"",ESAB);
            }
            m_Tmp = "";
        }
        else if(temp.split(" ").at(0) == "Copier")
        {
            Clavier("Ctrl+C");
            QClipboard *pP = QApplication::clipboard();
            tmp.start(1000);
            loop.exec();
            tmp.stop();
            emit Info("Copie de " + pP->text());
            if(pP->text().count() != 7)
            {
                err->Err(Traitement,"Copie PP échouée ! " + pP->text(),ESAB);
                return false;
            }
            if(temp.split(" ").at(1) == "Numero_BC_Esabora")
            {
                m_DB->Requete("UPDATE En_Cours SET Numero_BC_Esabora='" + pP->text() + "' WHERE Numero_Commande='" + bL.split("/").last().split(".").at(0) + "'");
            }
        }
        else if(temp.at(0) == '|')
        {
            etat = temp.split(" ").at(1).toInt();
        }
        else if(temp.contains(file)) { return true; }
        else if(temp != "")
        {
            if(temp.split(" ").last() == "oui" || temp.split(" ").last() == "non")
            {
                temp = temp.remove(temp.count() - (temp.split(" ").last().count() + 1),temp.count());
            }

            qDebug() << temp;
            Clavier(temp);
        }
        else err->Err(Traitement,temp,ESAB);
    }
    liste_Matos.clear();
    qDebug() << "Fin Esabora::Traitement_Fichier_Config()";
    return true;
}

void Esabora::Abort()
{
    qDebug() << "Esabora::Abort()";
    Traitement_Fichier_Config("Cancel");
    qDebug() << "Fin Esabora::Abort()";
}

bool Esabora::Fermeture_API()
{
    qDebug() << "Esabora::Fermeture_API()";
    Clavier("Echap");
    Clavier("Echap");
    Clavier("Echap");
    Clavier("Echap");
    Clavier("Echap");
    Clavier("Alt+F4");
    Clavier("Purge");
    p.close();
    qDebug() << "Fin Esabora::Fermeture_API()";
    return true;
}

bool Esabora::Clavier(QString commande)
{
    //OK, fonctionne uniquement au premier plan !
    //Lettre tjrs en majuscule !
    //keybd_event('O',0,0,0);
    //keybd_event('K',0,0,0);
    //keybd_event(VK_SPACE,0,0,0);
    //keybd_event(VK_SPACE,0,KEYEVENTF_KEYUP,0);
    qDebug() << "Esabora::Clavier()";
    if(commande == "Purge")
    {
        if(GetKeyState(VK_LMENU) < 0) { keybd_event(VK_LMENU,0,KEYEVENTF_KEYUP,0); }
        if(GetKeyState(VK_SHIFT) < 0) { keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0); }
        if(GetKeyState(VK_LCONTROL) < 0) { keybd_event(VK_LCONTROL,0,KEYEVENTF_KEYUP,0); }
        return true;
    }

    if(GetKeyState(VK_CAPITAL) < 0) { keybd_event(VK_CAPITAL,0,KEYEVENTF_KEYUP,0); }
    if(GetKeyState(VK_LMENU) < 0) { keybd_event(VK_LMENU,0,KEYEVENTF_KEYUP,0); }
    if(GetKeyState(VK_SHIFT) < 0) { keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0); }
    if(GetKeyState(VK_LCONTROL) < 0) { keybd_event(VK_LCONTROL,0,KEYEVENTF_KEYUP,0); }

    QEventLoop l;
    QTimer t;
    bool addition(true);
    connect(&t,SIGNAL(timeout()),&l,SLOT(quit()));

    QStringList ligne = commande.split("+");
    if(commande.at(0) == '-' && ligne.count() == 1)
    {
        ligne.clear();
        for(int cpt=1;cpt<=commande.count();cpt++)
        {
            ligne.append(commande.at(cpt));
        }
        addition = false;
    }

    int nbLigne = 0;
    while(ligne.count() > nbLigne)
    {
        QString var = ligne.at(nbLigne);
        //Verif majuscule
        if((var.at(0).isUpper() || var.at(0) == '/' || var.at(0) == '.') && GetKeyState(VK_SHIFT) >= 0 && GetKeyState(VK_LMENU) >= 0 &&
                addition == false)
        {
            keybd_event(VK_SHIFT,0,0,0);
        }
        else if((var.at(0).isLower() || var.at(0) == ':' || var.at(0) == '-' || var.at(0) == 'é' || var.at(0) == ',') && GetKeyState(VK_SHIFT) < 0)
        {
            keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
        }

        if(var == "Ctrl")
        {
            keybd_event(VK_LCONTROL,0,0,0);
        }
        else if(var == "Alt")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            if(GetKeyState(VK_LMENU) >= 0)
            {
                keybd_event(VK_LMENU,0,0,0);
            }
        }
        else if(var == "Tab")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_TAB,0,0,0);
            keybd_event(VK_TAB,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "Entrée")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_RETURN,0,0,0);
            keybd_event(VK_RETURN,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "Echap")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                    keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_ESCAPE,0,0,0);
            keybd_event(VK_ESCAPE,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "Gauche")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_LEFT,0,0,0);
            keybd_event(VK_LEFT,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "Droite")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_RIGHT,0,0,0);
            keybd_event(VK_RIGHT,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "Haut")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_UP,0,0,0);
            keybd_event(VK_UP,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "Bas")
        {
            if(GetKeyState(VK_SHIFT) < 0)
            {
                keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
            }
            keybd_event(VK_DOWN,0,0,0);
            keybd_event(VK_DOWN,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "F4")
        {
            keybd_event(VK_F4,0,0,0);
            keybd_event(VK_F4,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "A" || var == "a")
        {
            keybd_event('A',0,0,0);
        }
        else if(var == "B" || var == "b")
        {
            keybd_event('B',0,0,0);
        }
        else if(var == "C" || var == "c")
        {
            keybd_event('C',0,0,0);
        }
        else if(var == "D" || var == "d")
        {
            keybd_event('D',0,0,0);
        }
        else if(var == "E" || var == "e")
        {
            keybd_event('E',0,0,0);
        }
        else if(var == "é")
        {
            keybd_event('2',0,0,0);
        }
        else if(var == "F" || var == "f")
        {
            keybd_event('F',0,0,0);
        }
        else if(var == "G" || var == "g")
        {
            keybd_event('G',0,0,0);
        }
        else if(var == "H" || var == "h")
        {
            keybd_event('H',0,0,0);
        }
        else if(var == "I" || var == "i")
        {
            keybd_event('I',0,0,0);
        }
        else if(var == "J" || var == "j")
        {
            keybd_event('J',0,0,0);
        }
        else if(var == "K" || var == "k")
        {
            keybd_event('K',0,0,0);
        }
        else if(var == "L" || var == "l")
        {
            keybd_event('L',0,0,0);
        }
        else if(var == "M" || var == "m")
        {
            keybd_event('M',0,0,0);
        }
        else if(var == "N" || var == "n")
        {
            keybd_event('N',0,0,0);
        }
        else if(var == "O" || var == "o")
        {
            keybd_event('O',0,0,0);
        }
        else if(var == "P" || var == "p")
        {
            keybd_event('P',0,0,0);
        }
        else if(var == "Q" || var == "q")
        {
            keybd_event('Q',0,0,0);
        }
        else if(var == "R" || var == "r")
        {
            keybd_event('R',0,0,0);
        }
        else if(var == "S" || var == "s")
        {
            keybd_event('S',0,0,0);
        }
        else if(var == "T" || var == "t")
        {
            keybd_event('T',0,0,0);
        }
        else if(var == "U" || var == "u")
        {
            keybd_event('U',0,0,0);
        }
        else if(var == "V" || var == "v")
        {
            keybd_event('V',0,0,0);
        }
        else if(var == "W" || var == "w")
        {
            keybd_event('W',0,0,0);
        }
        else if(var == "X" || var == "x")
        {
            keybd_event('X',0,0,0);
        }
        else if(var == "Y" || var == "y")
        {
            keybd_event('Y',0,0,0);
        }
        else if(var == "Z" || var == "z")
        {
            keybd_event('Z',0,0,0);
        }
        else if(var == " ")
        {
            keybd_event(' ',0,0,0);
        }
        else if(var == "-")
        {
            keybd_event(54,0,0,0);
            keybd_event(54,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "/" || var == ":")
        {
            keybd_event(191,0,0,0);
            keybd_event(191,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == ".")
        {
            keybd_event(190,0,0,0);
            keybd_event(190,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == ",")
        {
            keybd_event(188,0,0,0);
            keybd_event(188,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "+")
        {
            keybd_event(187,0,0,0);
            keybd_event(187,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "0")
        {
            keybd_event(VK_NUMPAD0,0,0,0);
            keybd_event(VK_NUMPAD0,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "1")
        {
            keybd_event(VK_NUMPAD1,0,0,0);
            keybd_event(VK_NUMPAD1,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "2")
        {
            keybd_event(VK_NUMPAD2,0,0,0);
            keybd_event(VK_NUMPAD2,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "3")
        {
            keybd_event(VK_NUMPAD3,0,0,0);
            keybd_event(VK_NUMPAD3,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "4")
        {
            keybd_event(VK_NUMPAD4,0,0,0);
            keybd_event(VK_NUMPAD4,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "5")
        {
            keybd_event(VK_NUMPAD5,0,0,0);
            keybd_event(VK_NUMPAD5,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "6")
        {
            keybd_event(VK_NUMPAD6,0,0,0);
            keybd_event(VK_NUMPAD6,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "7")
        {
            keybd_event(VK_NUMPAD7,0,0,0);
            keybd_event(VK_NUMPAD7,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "8")
        {
            keybd_event(VK_NUMPAD8,0,0,0);
            keybd_event(VK_NUMPAD8,0,KEYEVENTF_KEYUP,0);
        }
        else if(var == "9")
        {
            keybd_event(VK_NUMPAD9,0,0,0);
            keybd_event(VK_NUMPAD9,0,KEYEVENTF_KEYUP,0);
        }
        else
        {
            emit Info("Erreur de lecture de " + var);
            qDebug() << "Clavier - Erreur de lecture de " << var << ", annulation";
            return false;
        }

        nbLigne++;
        t.start(100);
        l.exec();
    }
    nbLigne = 0;


    if(GetKeyState(VK_SHIFT) < 0)
    {
        keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
    }
    if(GetKeyState(VK_LMENU) < 0)
    {
        keybd_event(VK_LMENU,0,KEYEVENTF_KEYUP,0);
    }
    if(GetKeyState(VK_LCONTROL) < 0)
    {
        keybd_event(VK_LCONTROL,0,KEYEVENTF_KEYUP,0);
    }

    qDebug() << "Fin Esabora::Clavier()";
    return true;
}

bool Esabora::Souris(QString commande)
{
    qDebug() << "Esabora::Souris()";
    int x = 0,y = 0;
    bool clique = false;
    if(commande.split(" ").count() != 4) { return false; }
    x = commande.split(" ").at(1).toInt();
    y = commande.split(" ").at(2).toInt();
    if(commande.split(" ").at(3) == "oui")
    {
        clique = true;
    }
    if(SetCursorPos(x,y) == false) { return false; }
    if(clique)
    {
        mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);
        mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
    }
    qDebug() << "Fin Esabora::Souris()";
    return true;
}

int Esabora::GetEtat()
{
    return etat;
}

bool Esabora::Verification_Fenetre(QString fenetre)
{
    qDebug() << "Esabora::Verification_Fenetre()";
    HWND hWnds = FindWindow(NULL,fenetre.toStdWString().c_str());
    if(hWnds == NULL)
    {
        hWnds = FindWindow(NULL,L"AVERTISSEMENT");
        if(hWnds == NULL) { return false; }
        else
        {
            Clavier("Entrée");
            QEventLoop loop;
            QTimer t;
            connect(&t,SIGNAL(timeout()),&loop,SLOT(quit()));
            t.start(2000);
            loop.exec();
            if(Verification_Fenetre(fenetre)) {
                DEBUG << "La fenêtre '" << fenetre << "' a pas été trouvé(Ok)";
                return true;
            }
            else {
                DEBUG << "La fenêtre '" << fenetre << "' n'a pas été trouvé(Erreur)";
                return false;
            }
        }
    }
    else return true;
}

bool Esabora::Verification_Focus(QString fen, bool focus)
{
    qDebug() << "Esabora::Verification_Focus()";
    QString mess;
    DEBUG << "Détection fenêtre : " << Verification_Message_Box(mess);
    QEventLoop loop;
    QTimer t;
    connect(&t,SIGNAL(timeout()),&loop,SLOT(quit()));
    HWND hWnds = FindWindow(NULL,fen.toStdWString().c_str());
    HWND h = GetForegroundWindow();
    if(hWnds == h && focus) {
        DEBUG << "La fenêtre '" << fen << "' est toujours au premier plan(Ok)";
        return true;
    }
    else if(hWnds != h && focus == false)
    {
        QString message;
        DEBUG << "Détection fenêtre : " << Verification_Message_Box(message);
        DEBUG << "La fenêtre '" << fen << "' n'est plus au premier plan(Ok)";
        return true;
    }
    else if(hWnds != h && focus)
    {
        QString message;
        DEBUG << "Détection fenêtre : " << Verification_Message_Box(message);
        if(Verification_Fenetre("AVERTISSEMENT"))
        {
            DEBUG << "'AVERTISSEMENT' Detected !";
            Clavier("Entrée");
            t.start(2000);
            loop.exec();
        }
        if(Verification_Fenetre("RX - Agent de Mise à Jour"))
        {
            DEBUG << "'UPDATE Widget' Detected !";
            err->Err(Focus,"RX - Agent de mise à jour",ESAB);
        }
        DEBUG << "La fenêtre '" << fen << "' n'est plus au premier plan(Erreur)";
        return false;
    }
    else {
        DEBUG << "La fenêtre '" << fen << "' est toujours au premier plan(Erreur)";
        return false;
    }
}
///Erreur 6xx
bool Esabora::Ajout_Stock(QString numero_Commande)
{
    DEBUG << "Esabora::Ajout_Stock()";
    if(Traitement_Fichier_Config("Ajout_Stock","///" + numero_Commande + ".tmp") == false)
    {
        err->Err(Traitement,"",ESAB);
        DEBUG << "Ajout_Stock - Echec dans le traitement du Bon de commande";
        return false;
    }
    DEBUG << "Fin Esabora::Ajout_Stock()";
    return true;
}

void Esabora::Semi_Auto(QString NumeroCommande)
{
    qDebug() << "Esabora::Semi_Auto()";
    QSqlQuery r = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='23'");
    QSqlQuery r2 = m_DB->Requete("SELECT Valeur FROM Options WHERE ID='12'");
    r.next();
    r2.next();
    if(Verification_Fenetre(r.value("Valeur").toString() + " - SESSION : 1 - REPERTOIRE : " + r2.value("Valeur").toString()) == false)
    {
        emit Message("Erreur","Esabora n'est pas ouvert !",true);
        return;
    }
    else
    {
        QString fen(r.value("Valeur").toString() + " - SESSION : 1 - REPERTOIRE : " + r2.value("Valeur").toString());
        HWND hWnds = FindWindow(NULL,fen.toStdWString().c_str());
        SetForegroundWindow(hWnds);
    }
    Traitement_Fichier_Config("Semi_Auto","///" + NumeroCommande + ".");

    if(QMessageBox::question(m_fen,"","L'ajout du bon de commande à t'il réussi ?") == QMessageBox::Yes)
    {
        m_DB->Requete("UPDATE En_Cours SET Ajout='"+QString::number(endAdd)+"' WHERE Numero_Commande='" + NumeroCommande + "'");
    }

    emit Message("","Le bon de livraison doit être validé manuellement.",false);
    qDebug() << "Fin Esabora::Semi_Auto()";
}

void Esabora::Set_Liste_Matos(QStringList liste)
{
    qDebug() << "Esabora::Set_Liste_Matos()";
    liste_Matos = liste;
    qDebug() << liste_Matos;
    qDebug() << "Fin Esabora::Set_Liste_Matos()";
}

void Esabora::Reset_Liste_Matos()
{
    liste_Matos.clear();
}

void Esabora::Apprentissage(QString &entreprise,QString &BDD)
{
    qDebug() << "Esabora::Apprentissage()";   
    QProcess p;
    if(m_Lien_Esabora.contains("Simul_Esabora"))
    {
        p.setProgram(m_Lien_Esabora);
        p.start();
    }
    else
        QDesktopServices::openUrl(m_Lien_Esabora);

    if(Lancement_API())
    {
        HWND hwnd = GetForegroundWindow();
        LPWSTR buf;
        buf = new WCHAR[128];
        if(GetWindowTextW(hwnd,buf,350))
        {
            QString var;
            var = var.fromStdWString(buf);
            if(var.contains("REPERTOIRE") && var.contains("SESSION"))
            {
                QStringList v = var.split(" - ");
                entreprise = v.at(0);
                BDD = v.at(2).split(" : ").at(1);
            }
        }
    }
    Clavier("Alt+F4");
    qDebug() << "Fin Esabora::Apprentissage()";
}

void Esabora::Set_Var_Esabora(QString lien,QString login,QString MDP)
{
    qDebug() << "Esabora::Set_Vat_Esabora()";
    m_Lien_Esabora = lien;
    m_Login = login;
    m_MDP = MDP;
    qDebug() << "Fin Esabora::Set_Var_Esabora()";
}

bool Esabora::Verification_Message_Box(QString &message)
{
    qDebug() << "Esabora::Verification_Message_Box()";
    HWND Handle = GetForegroundWindow(); //Récupération du handle

    RECT rect;
    GetWindowRect(Handle,&rect);
    //Screen
    QPixmap originalPixmap;
    QScreen *screen = QGuiApplication::primaryScreen();
    if(screen)
    {
        originalPixmap = screen->grabWindow(0);
        originalPixmap = originalPixmap.copy(rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top);
        DEBUG << "Pixmap size : " << originalPixmap.width() << "x" << originalPixmap.height();
    }
    else
        return false;
    //enregistrement screen
    QDir d;
    d.setPath(m_Lien_Work + "/Temp");
    int nb = d.entryList(QDir::NoDotAndDotDot | QDir::Files).count() + 1;
    QFile file(m_Lien_Work + "/Temp/" + QString::number(nb) + ".PNG");
    DEBUG << "File Open : " << file.open(QIODevice::WriteOnly);
    DEBUG << "Save Pixmap : " << m_Lien_Work + "/Temp/" + QString::number(nb) + ".PNG" << originalPixmap.save(&file,"PNG");
    //Accès screen comparatifs
    QDir dir;
    dir.setPath(m_Lien_Work + "/Screen/");
    QImage img(0),img2(0);
    DEBUG << "Load Image : " << img.load(m_Lien_Work + "/Temp/" + QString::number(nb) + ".PNG");
    //Boucle suivant le nombre de fichiers dans le dossier
    for(int cpt=0;cpt<dir.entryList(QDir::NoDotAndDotDot | QDir::Files).count();cpt++)
    {
        img2.load(dir.entryList(QDir::NoDotAndDotDot | QDir::Files).at(cpt));
        if(img.operator ==(img2))//Si les screens correspondent
        {
            QSqlQuery req = m_DB->Requete("SELECT Valeur FROM Options WHERE Nom='MBText_" + QString::number(nb) + "'");
            if(req.next())
            {
                DEBUG << "MessageBox = " << req.value(0);
            }
            else
            {
                emit Info("DEBUG : MessageBox introuvable ! " + dir.entryList(QDir::NoDotAndDotDot | QDir::Files).at(cpt));
            }
            file.remove();
            cpt = dir.entryList(QDir::NoDotAndDotDot | QDir::Files).count();
            qDebug() << "Fin Esabora::Verification_Message_Box()";
            return true;
        }
    }
    //Si aucune screen ne correspond
    emit Info("DEBUG : MessageBox inconnue !");
    message = "Inconnu";
    qDebug() << "Verification_Message_Box - MessageBox inconnue";
    qDebug() << "Fin Esabora::Verification_Message_Box()";
    return false;
}

bool Esabora::Get_List_Matos(QString invoice)
{
    emit DemandeListeMatos(invoice);
    QEventLoop l;
    QTimer t;
    connect(&t,SIGNAL(timeout()),&l,SLOT(quit()));
    connect(this,SIGNAL(ReceptionListeMatos()),&l,SLOT(quit()));
    t.start(60000);
    //l.exec();

    if(t.isActive()) { return true; }
    else { return false; }
}

void Esabora::Stop()
{
    m_Arret = true;
}

QString Esabora::Test_Find_Fabricant(QString fab)
{
    return Find_Fabricant(fab);
}

bool Esabora::Copy()
{
    QEventLoop l;
    QTimer t;
    connect(&t,SIGNAL(timeout()),&l,SLOT(quit()));
    QClipboard *cb = QApplication::clipboard();
    cb->clear();
    t.start(1000);
    l.exec();
    bool test = Clavier("Ctrl+C");
    t.start(1000);
    l.exec();
    return test;
}

bool Esabora::Paste()
{
    QEventLoop l;
    QTimer t;
    connect(&t,SIGNAL(timeout()),&l,SLOT(quit()));
    bool test = Clavier("Ctrl+V");
    t.start(1000);
    l.exec();
    return test;
}

QStringList Esabora::Verif_List(QStringList list)
{
    DEBUG << "Esabora | Verif_List";
    DEBUG << list << list.count();
    if(list.isEmpty() || list.count() % 7 != 0)
    {
        DEBUG << "Liste matériels Vide !";
        return QStringList(0);
    }

    QFile file(m_Lien_Work + "/Config/Fab.esab");
    if(file.open(QIODevice::ReadOnly) == false)
    {
        err->Err(open_File,"Fab.esab",ESAB);
    }
    QTextStream flux(&file);

    QStringList final;
    for(int i=0;i<list.count()-1;i+=7)
    {
        final.append(list.at(i));
        final.append(list.at(i+1));
        final.append(list.at(i+2));
        if(list.at(i+3).isEmpty() && list.at(i+2).isEmpty() == false)//si Fabricant connu mais pas Fab
        {
            flux.seek(0);
            DEBUG << "If";
            while(flux.atEnd() == false)//vérif si Fab déjà connu
            {
                QString var = flux.readLine();
                if(var.split(";").count() == 2 && var.split(";").at(0) == list.at(i+2))
                {
                    final.append(var.split(";").at(1));
                    file.seek(0);
                    break;
                }
            }
            if(flux.atEnd())//Sinon rechercher Fab sur esabora
            {
                QString var = Find_Fabricant(list.at(i+2));
                if(var.isEmpty() == false)
                {
                    file.seek(SEEK_END);
                    flux << list.at(i+2) + ";" + var + "\r\n";
                    file.seek(0);
                }
                final.append(var);
            }
        }
        else
        {
            file.seek(0);
            DEBUG << "Else";
            QString var = flux.readAll();
            DEBUG << "Control existing row";
            if(list.at(i+2).isEmpty() == false)
            {
                if(var.contains(list.at(i+2) + ";" + list.at(i+3)) == false)
                {
                    DEBUG << "Add New entry";
                    file.close();
                    if(file.open(QIODevice::WriteOnly | QIODevice::Append) == false)
                        err->Err(open_File,"Fab.esab",ESAB);
                    flux << list.at(i+2) + ";" + list.at(i+3) + "\r\n";
                    file.waitForBytesWritten(10000);
                    file.close();
                    if(file.open(QIODevice::ReadOnly) == false)
                        err->Err(open_File,"Fab.esab",ESAB);
                }
            }
            else
            {
                DEBUG << "Erreur, fabricant non trouvé dans la commande";
            }
            final.append(list.at(i+3));
        }
        final.append(list.at(i+4));
        final.append(list.at(i+5));
        final.append(list.at(i+6));
    }
    DEBUG << "Esabora | Fin Verif_List";
    return final;
}

void Esabora::Test_Add_BC(QString invoice)
{
    if(Get_List_Matos(invoice) == false)
    {
        DEBUG << "Echec, liste matériels vide !";
        return;
    }
    if(Lancement_API())
    {
        if(Ouverture_Liste_BC())
        {
            if(Ajout_BC(invoice))
            {
                DEBUG << "Test réussis !";
                m_DB->Requete("UPDATE En_Cours SET Ajout='" + QString::number(add) + "' WHERE Numero_Commande='" + invoice + "'");
            }
            else
            {
                DEBUG << "Echec Ajout BC";
                m_DB->Requete("UPDATE En_Cours SET Ajout='"+QString::number(error)+"' WHERE Numero_Commande='" + invoice + "'");
            }
        }
    }
    else
    {
        DEBUG << "Echec démarrage Esabora";
    }
    Abort();
    Fermeture_API();
}

void Esabora::Test_Add_BL(QString invoice,QString bl)
{
    if(Lancement_API())
    {
        if(Ajout_BL(invoice,bl))
        {
            DEBUG << "Test réussis !";  
        }
        else
        {
            DEBUG << "Echec Ajout BC";

        }
    }
    else
    {
        DEBUG << "Echec démarrage Esabora";
    }
    Abort();
    Fermeture_API();
}

