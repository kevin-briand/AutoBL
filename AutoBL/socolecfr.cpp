#include "socolecfr.h"

SocolecFr::SocolecFr(FctFournisseur *fct, const QString login, const QString mdp, const QString lien_Travail, const QString comp, DB *db):
    m_Login(login),m_MDP(mdp),m_UserName(comp),m_WorkLink(lien_Travail),m_Fct(fct),m_DB(db)
{
    DEBUG << "Init Class " << FRN;
}

bool SocolecFr::Start()
{
    DEBUG << "Start " << FRN;

    bool firstInit(false);
    bool error(false);

    QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Fournisseur='" + QString(FRN) + "'");
    if(req.next() == false)
    {
        firstInit = true;
    }

    if(Connexion())
    {
        m_Fct->Info("Chargement des commandes...");
        DEBUG << "Socolec | Connected";
        if(Create_List_Invoice(firstInit) == false) { error = true; }

        //Update State
        DEBUG << "Socolec | Update State";
        req = m_DB->Requete("SELECT * FROM En_Cours WHERE Fournisseur='" + QString(FRN) + "' AND (Etat='" + QString::number(open) + "' OR "
                                                                                                 "Etat='" + QString::number(partial) + "')");
        while(req.next())
        {
            m_Fct->Info(tr("Mise à jour état commande %0").arg(req.value("Numero_Commande").toString()));
            Update_State(req.value("Numero_Commande").toString());
        }

        //Update Delivery
        DEBUG << "Socolec | Update Delivery";
        req = m_DB->Requete("SELECT * FROM En_Cours WHERE Fournisseur='" + QString(FRN) + "' AND Numero_Livraison='' AND Etat ='" + QString::number(Close) + "'");
        while(req.next())
        {
            m_Fct->Info(tr("Récupération des bons de livraison commande %1").arg(req.value("Numero_Commande").toString()));
            Update_Delivery(req.value("Numero_COmmande").toString());
        }
    }
    else
        return false;

    if(error) { return false; }
    return true;
}

bool SocolecFr::Connexion()
{
    //Chargement de la page
    m_Fct->Info("Connexion...");
    if(m_Fct->WebLoad("https://socolec.sonepar.fr/is-bin/INTERSHOP.enfinity/WFS/Sonepar-SOCOLEC-Site/fr_FR/-/EUR/ViewLogin-Start") == false)
    {
        m_Fct->FrnError(load,FRN,"Connexion");
        return false;
    }

    //Injection des scripts de connexion
    m_Fct->InsertJavaScript("document.getElementsByName('LoginForm_Login')[0].value = '" + m_Login + "';");
    m_Fct->InsertJavaScript("document.getElementsByName('LoginForm_Password')[0].value = '" + m_MDP + "';");
    m_Fct->InsertJavaScript("document.getElementsByName('LoginForm_RegistrationDomain')[0].value = '" + m_UserName + "';");
    m_Fct->InsertJavaScript("document.getElementsByName('LoginForm')[0].submit();");
    m_Fct->Loop(5000);

    //Contrôle de connexion
    if(m_Fct->FindTexte("Le N° de compte spécifié n'existe pas"))
    {
        m_Fct->FrnError(bad_Login,FRN,"Numéro de compte inconnu");
    }
    else if(m_Fct->FindTexte("Email ou mot de passe incorrect"))
    {
        m_Fct->FrnError(bad_Login,FRN);
    }
    else if(m_Fct->FindTexte("Historique des commandes"))
    {
        return true;
    }
    return false;
}

bool SocolecFr::Create_List_Invoice(bool firstInit)
{
    //Chargement de la page
    //https://socolec.sonepar.fr/is-bin/INTERSHOP.enfinity/WFS/Sonepar-SOCOLEC-Site/fr_FR/-/EUR/ViewPurchaseOrderList-Start?SortAttribute=CreationDate&AttributeType=DATE&SortOrder=false&ListAllOrders=1&PageNumber=1
    if(m_Fct->WebLoad("https://socolec.sonepar.fr/is-bin/INTERSHOP.enfinity/WFS/Sonepar-SOCOLEC-Site/fr_FR/-/EUR/"
                       "ViewPurchaseOrderList-Start?SortAttribute=CreationDate&AttributeType=DATE&SortDirection=DESC&ListAllOrders=1")  == false)
    {
        m_Fct->FrnError(load,FRN,"Liste des commandes");
        return false;
    }

    //Enregistrement de la page
    m_Fct->SaveHtml();

    //Traitement des informations    
    bool endScan(false);
    int next(0);
    while(endScan == false)
    {
        QString text,link,name,ref,date,etat,invoice_Number,nextPage;

        //Ouverture de la page
        QFile f(m_WorkLink + "/web_Temp.txt");
        if(f.open(QIODevice::ReadOnly) == false)
        {
            m_Fct->FrnError(open_File,FRN,f.fileName());
            return false;
        }
        QTextStream flux(&f);

        while(f.atEnd() == false)
        {
            bool skip = false;
            text = flux.readLine();
            if(text.contains("tr style=\"cursor: pointer;\""))//Point de départ d'une ligne du tableau
            {
                if(text.split("'").count() > 1)
                {
                    link = text.split("'").at(1);
                }
                else
                {
                    m_Fct->FrnError(variable,FRN,"link");
                    return false;
                }
                text = flux.readLine();
                if(text.split(">").count() > 1)
                {
                    text = text.split(">").at(1).split("<").at(0).split("&").at(0);
                    date = text.split("/").at(2) + "-" + text.split("/").at(1) + "-" + text.split("/").at(0);
                }
                else
                {
                    m_Fct->FrnError(variable,FRN,"date");
                    return false;
                }

                flux.readLine();
                flux.readLine();
                flux.readLine();
                flux.readLine();
                flux.readLine();
                text = flux.readLine();
                if(text.split(">").count() > 3)//Ref chantier
                {
                    ref = text.split(">").at(2).split("<").at(0);
                }
                else
                {
                    m_Fct->FrnError(variable,FRN,"Référence chantier");
                    return false;
                }
                text = flux.readLine();
                if(text.split(">").count() > 3)//Nom chantier
                {
                    name = text.split(">").at(2).split("<").at(0);
                }
                else
                {
                    DEBUG << "Pas de nom de chantier";
                }
                flux.readLine();
                flux.readLine();
                text = flux.readLine();
                text.replace("\t","");
                DEBUG << "Ajout commande " << text;
                invoice_Number = text;
                flux.readLine();
                flux.readLine();
                text = flux.readLine();
                if(text.split(">").count() > 1)
                {
                    text = text.split(">").at(1);
                    text.replace("Ã©","é");
                    if(text == "En attente" || text == "En traitement" || text == "En préparation")
                    {
                        etat = "0";
                    }
                    else if(text == "Partiellement livrée" || text == "Partiellement facturée")
                    {
                        etat = "1";
                    }
                    else if(text == "Livrée" || text == "Facturée" || text == "Terminée")
                    {
                        etat = "2";
                    }
                    else if(text == "Annulée")
                    {
                        skip = true;
                    }
                    else
                    {
                        m_Fct->FrnError(variable,FRN,"Valeur Etat");
                        return false;
                    }
                }
                else
                {
                    m_Fct->FrnError(variable,FRN,"Etat");
                    return false;
                }
                QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + invoice_Number + "' AND Fournisseur='" + FRN + "'");
                bool exist = req.next();
                if(skip == false && exist == false)
                {
                    int ID(0);
                    req = m_DB->Requete("SELECT MAX(ID) FROM En_Cours");
                    req.next();
                    ID = req.value(0).toInt();
                    ID++;
                    m_DB->Requete("INSERT INTO En_Cours VALUES('" + QString::number(ID) + "','" + date + "','" + ref + "','" + invoice_Number + "','','" + link + "','" + etat + "','','" + name + "','0','','" + FRN + "')");
                    if(firstInit)
                    {
                        m_DB->Requete("UPDATE En_Cours SET Ajout='"+QString::number(endAdd)+"' WHERE ID='" + QString::number(ID) + "'");
                        return true;
                    }
                }
                else if(exist == true)
                {
                    endScan = true;
                    break;
                }
            }
            else if(text.contains("Suivant"))
            {
                if(text.split("\"").count() > 1)
                {
                    nextPage = text.split("\"").at(1);
                }
            }
        }

        //Next Page
        f.close();
        if(endScan == false)
        {
            next++;
            if(m_Fct->WebLoad("https://socolec.sonepar.fr/is-bin/INTERSHOP.enfinity/WFS/Sonepar-SOCOLEC-Site/fr_FR/-/EUR/"
                               "ViewPurchaseOrderList-Start?SortAttribute=CreationDate&AttributeType=DATE&SortDirection=DESC&ListAllOrders=1&PageNumber=" + QString::number(next))  == false)
            {
                m_Fct->FrnError(load,FRN,"Liste des commandes");
                return false;
            }
            m_Fct->SaveHtml();
        }
    }
    return false;
}

bool SocolecFr::Update_Delivery(QString invoice)
{
    //Récup info dans DB
    QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + invoice + "' AND Fournisseur='" + FRN + "'");
    if(req.next() == false)
    {
        m_Fct->FrnError(requete,FRN,"Update_Delivery");
    }
    else
    {
        QString link = req.value("Lien_Commande").toString();
        link.replace("Start","ViewBL");
        link = link + "&CodeCommande=" + invoice;
        if(m_Fct->WebLoad(link) == false)
        {
            m_Fct->FrnError(load,FRN);
        }
        else
        {
            if(m_Fct->FindTexte(invoice) == false || m_Fct->FindTexte("BL N") == false)
            {
                m_Fct->FrnError(fail_check,FRN,"Page non chargée");
            }
            else
            {
                if(m_Fct->SaveHtml() == false)
                {
                    m_Fct->FrnError(save_file,FRN,"Html");
                }
                else
                {
                    QString bl;

                    QFile f(m_WorkLink + "/web_Temp.txt");
                    if(f.open(QIODevice::ReadOnly) == false)
                    {
                        m_Fct->FrnError(open_File,FRN,"web_Temp");
                    }
                    else
                    {
                        QTextStream flux(&f);
                        while(flux.atEnd() == false)
                        {
                            bl = flux.readLine();
                            if(bl.contains("BL N<sup>o</sup>"))
                            {
                                bl = bl.split(">").last().split(" ").at(1);
                                break;
                            }
                        }
                        m_DB->Requete("UPDATE En_Cours SET Numero_Livraison='" + bl + "' WHERE Numero_Commande='" + invoice + "' AND Fournisseur='" + FRN + "'");
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool SocolecFr::Update_State(QString invoice)
{
    //Récup info dans DB
    QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + invoice + "' AND Fournisseur='" + FRN + "'");
    if(req.next() == false)
    {
        m_Fct->FrnError(requete,FRN,"Update_Delivery");
    }
    else
    {
        if(m_Fct->WebLoad(req.value("Lien_Commande").toString()) == false)
        {
            m_Fct->FrnError(load,FRN);
        }
        else
        {
            if(m_Fct->FindTexte(invoice) == false)
            {
                m_Fct->FrnError(fail_check,FRN,"Page non chargée");
            }
            else
            {
                int state(0);

                if(m_Fct->FindTexte("En attente")) { state = open; }
                else if(m_Fct->FindTexte("En traitement")) { state = open; }
                else if(m_Fct->FindTexte("En préparation")) { state = open; }
                else if(m_Fct->FindTexte("Livrée")) { state = Close; }
                else if(m_Fct->FindTexte("Partiellement livrée")) { state = partial; }
                else if(m_Fct->FindTexte("Partiellement facturée")) { state = partial; }
                else if(m_Fct->FindTexte("Facturée")) { state = Close; }
                else if(m_Fct->FindTexte("Terminée")) { state = Close; }
                else if(m_Fct->FindTexte("Annulée"))
                {
                    m_DB->Requete("DELETE FROM En_Cours WHERE Numero_Commande='" + invoice + "' AND Fournisseur='" + FRN + "'");
                }

                if(state != 0)
                {
                    m_DB->Requete("UPDATE En_Cours SET Etat='" + QString::number(state) + "' WHERE Numero_Commande='" + invoice + "' AND Fournisseur='" + FRN + "'");
                }
                return true;
            }
        }
    }
    return false;
}

QStringList SocolecFr::Get_Invoice(const QString InvoiceNumber)
{
    //Retourne une liste d'un tableau de commande
    //0 = nb commande
    //boucle de 7 strings designation,reference,fabricant,fab,prix unitaire,quantité livré,quantité restante

    DEBUG << "Socolec.fr | Connexion";
    m_Fct->Change_Load_Window(tr("Connexion..."));
    if(Connexion() == false) { return QStringList(0); }

    m_Fct->Change_Load_Window(tr("Chargement de la commande..."));
    DEBUG << "Socolec.fr | Récupération du lien";
    QSqlQuery req = m_DB->Requete("SELECT * FROM En_Cours WHERE Numero_Commande='" + InvoiceNumber + "' AND Fournisseur='" + FRN + "'");
    if(req.next() == false)
    {
        m_Fct->FrnError(requete,FRN,InvoiceNumber);
    }
    else
    {
        DEBUG << "Socolec.fr | Chargement de la page";
        if(m_Fct->WebLoad(req.value("Lien_Commande").toString()) == false)
        {
            m_Fct->FrnError(load,FRN,req.value("Lien_Commande").toString());
        }
        else
        {
            DEBUG << "Socolec.fr | Vérification de la page";
            if(m_Fct->FindTexte(InvoiceNumber) == false)
            {
                m_Fct->FrnError(fail_check,FRN,InvoiceNumber);
            }
            else
            {
                DEBUG << "Socolec.fr | Traitement des informations de la page";
                if(m_Fct->SaveHtml() == false)
                {
                    m_Fct->FrnError(save_file,FRN,"Html");
                }
                else
                {
                    ///Traitement des données de la page
                    QFile file(m_WorkLink + "/web_Temp.txt");
                    if(file.open(QIODevice::ReadOnly) == false)
                    {
                        m_Fct->FrnError(open_File,FRN,"web_Temp.txt");
                    }
                    else
                    {
                        QTextStream flux(&file);
                        int etat(0);
                        QStringList list;
                        while(flux.atEnd() == false)
                        {
                            QString var = flux.readLine();
                            if(var.contains("obj[CodeEnseigne].LibCourt") && etat == 0)//Désignation
                            {
                                var.replace("&amp;","ET");
                                list.append(var.split("\"").at(1));
                                etat = 1;
                            }
                            else if(var.contains("obj[CodeEnseigne].ManufacturerName") && etat == 1)//référence + Fabricant
                            {
                                QString var2 = flux.readLine();
                                if(var2.contains("obj[CodeEnseigne].ManufacturerSKU"))
                                {
                                    if(var.split("\"").count() >= 2 && var2.split("\"").count() >= 2)
                                    {
                                        var.replace("&amp;","ET");
                                        var2.replace("&amp;","ET");
                                        list.append(var2.split("\"").at(1));//Référence
                                        list.append(var.split("\"").at(1));//Fabricant
                                        list.append("");//Fab
                                        etat = 2;
                                    }
                                    else
                                    {
                                        m_Fct->FrnError(variable,FRN,"Référence");
                                    }
                                }
                            }
                            else if(var.contains("<input type=\"hidden\" name=\"QuantityList_") && etat == 2)//Prix + quantité + restant
                            {
                                QString var2 = var;
                                flux.readLine();
                                flux.readLine();
                                while(flux.readLine().contains("<td style=\"text-align:right;\">") == false) {}
                                var = flux.readLine();
                                var.replace(" ","");
                                var.replace(",",".");
                                var.replace("&nbsp;","");
                                var2.replace(",",".");
                                var = QString::number(var.toDouble() / var2.split("\"").at(5).toDouble());
                                var.replace(".",",");

                                if(var == "nan")//Si annulé
                                {
                                    list.removeLast();
                                    list.removeLast();
                                    list.removeLast();
                                    list.removeLast();
                                }
                                else
                                {
                                    list.append(var);//prix
                                    if(var2.split("\"").count() >= 6)
                                    {
                                        list.append(var2.split("\"").at(5));//Quantité
                                    }
                                    else
                                    {
                                        m_Fct->FrnError(variable,FRN,"Quantité");
                                    }
                                    list.append("NC");//Restant
                                }
                                etat = 0;
                            }
                        }
                        if(etat != 0)
                        {
                            m_Fct->FrnError(variable,FRN,"Création tableau(Etat = " + QString::number(etat) + ")");
                        }
                        else
                        {
                            DEBUG << list;
                            return list;
                        }
                    }
                }
            }
        }
    }
    return QStringList(0);
}

void SocolecFr::Set_Var(const QString login, const QString mdp, const QString comp)
{
    m_Login = login;
    m_MDP = mdp;
    m_UserName = comp;
}

bool SocolecFr::Test_Connexion()
{
    DEBUG << "TEST CONNEXION " << FRN;
    return Connexion();
}

QString SocolecFr::Get_Inf()
{
    return QString(INF);
}
