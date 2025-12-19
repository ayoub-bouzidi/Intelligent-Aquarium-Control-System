// ==================================================
//   CONFIG LCD
// ==================================================
sbit LCD_RS at RD0_bit;
sbit LCD_EN at RD1_bit;
sbit LCD_D4 at RD2_bit;
sbit LCD_D5 at RD3_bit;
sbit LCD_D6 at RD4_bit;
sbit LCD_D7 at RD5_bit;

sbit LCD_RS_Direction at TRISD0_bit;
sbit LCD_EN_Direction at TRISD1_bit;
sbit LCD_D4_Direction at TRISD2_bit;
sbit LCD_D5_Direction at TRISD3_bit;
sbit LCD_D6_Direction at TRISD4_bit;
sbit LCD_D7_Direction at TRISD5_bit;

// ==================================================
//   BROCHES POUR MODE CONTROLE ECLAIRAGE/POMPE
// ==================================================
#define LDR_DIGITAL     RB0_bit
#define LED_TEMOIN      RC0_bit
#define POMPE_CONTROLE  RC3_bit

// ==================================================
//   BROCHES POUR MODE AQUARIUM
// ==================================================
#define LAMPE_1         RC0_bit
#define LAMPE_2         RC1_bit
#define LAMPE_3         RC5_bit
#define VENTILATEUR     RC4_bit
#define CHAUFFAGE       RC3_bit
#define LAMPE_4         RC6_bit
#define BUZZER          RC2_bit
#define POMPE_AQUARIUM  RC7_bit

// ==================================================
//   VARIABLES
// ==================================================
volatile unsigned char mode_jour_nuit = 0;
volatile unsigned char mode_eclairage_manuel = 0;
volatile unsigned char etat_eclairage = 0;
volatile unsigned char mode_pompe_manuel = 0;
volatile unsigned char etat_pompe = 0;
volatile unsigned char mode_maintenance = 0;

// Variable pour le mode actuel (0=Contrôle, 1=Aquarium)
unsigned char mode_actuel = 0;
unsigned char bouton_precedent = 1;

// Variables pour stabiliser l'affichage
unsigned char affichage_initialise = 0;
unsigned int temp_prec = 0, niveau_prec = 0, heure_prec = 0;
unsigned char periode_prec = 0;

// Variables pour l'aquarium
unsigned char alarme_active = 0;
unsigned int compteur_alarme = 0;
unsigned char pompe_active = 0;

// Variables pour le comptage d'appuis sur RA4 et Timer0
unsigned char compteur_appuis_RA4 = 0;
unsigned char affichage_seuils_active = 0;
unsigned int compteur_2s = 0;

// Variables pour les messages d'état aquarium
unsigned char afficher_message = 0;
char message_lcd[17];
unsigned int compteur_message = 0;

// Seuils pour l'aquarium
#define SEUIL_TEMP_HAUT 25
#define SEUIL_TEMP_BAS  10
#define SEUIL_NIVEAU_BAS 50
#define SEUIL_NIVEAU_HAUT 80
#define SEUIL_NIVEAU_CRITIQUE 20

// Plages horaires pour aquarium (ANCIENNE LOGIQUE)
#define HEURE_DEBUT_JOUR 7
#define HEURE_FIN_JOUR   19

// ==================================================
//   FONCTIONS POUR L'AQUARIUM
// ==================================================
void AfficherNombre(unsigned int nombre, char x, char y) {
    char txt[4];
    txt[0] = '\0';

    if (nombre < 10) {
        txt[0] = ' ';
        txt[1] = '0' + nombre;
        txt[2] = '\0';
    } else if (nombre < 100) {
        txt[0] = '0' + (nombre/10);
        txt[1] = '0' + (nombre%10);
        txt[2] = '\0';
    } else {
        txt[0] = '0' + (nombre/100);
        txt[1] = '0' + ((nombre/10)%10);
        txt[2] = '0' + (nombre%10);
        txt[3] = '\0';
    }

    if (txt[0] != '\0') {
        Lcd_Out(y, x, txt);
    }
}

// Fonction pour afficher un nombre simplement
void AfficherNb(unsigned int nb, char x, char y) {
    char d1, d2;

    if (nb < 10) {
        Lcd_Chr(y, x, ' ');
        Lcd_Chr(y, x+1, '0' + nb);
    } else if (nb < 100) {
        d1 = nb / 10;
        d2 = nb % 10;
        Lcd_Chr(y, x, '0' + d1);
        Lcd_Chr(y, x+1, '0' + d2);
    } else {
        // Pour les nombres à 3 chiffres
        Lcd_Chr(y, x, '0' + (nb/100));
        Lcd_Chr(y, x+1, '0' + ((nb/10)%10));
        Lcd_Chr(y, x+2, '0' + (nb%10));
    }
}

unsigned int lire_niveau_moyenne() {
    unsigned int somme = 0;
    unsigned char i;
    for(i=0; i<4; i++) {
        somme += ADC_Read(0);
        Delay_ms(10);
    }
    return somme / 4;
}

// ==================================================
//   FONCTIONS POUR AFFICHAGE SEUILS ET MESSAGES
// ==================================================

// Fonction pour afficher les seuils
void AfficherSeuilsSysteme() {
    Lcd_Cmd(_LCD_CLEAR);
    Lcd_Out(1, 1, "Seuil 1 : ");
    AfficherNb(SEUIL_TEMP_HAUT, 11, 1);  // Afficher seuil température haut
    Lcd_Out(1, 13, "C");
    
    Lcd_Out(2, 1, "Seuil 2 : ");
    AfficherNb(SEUIL_TEMP_BAS, 11, 2);   // Afficher seuil température bas
    Lcd_Out(2, 13, "C");
}

// Fonction pour afficher un message temporaire sur LCD
void AfficherMessageTemp(char *msg, unsigned int duree_ms) {
    Lcd_Cmd(_LCD_CLEAR);
    Lcd_Out(1, 1, msg);
    Delay_ms(duree_ms);
    
    // Réinitialiser l'affichage
    affichage_initialise = 0;
}

// Fonction pour détecter les appuis sur RA4
void GererAppuisRA4() {
    static unsigned char etat_precedent = 1;
    
    // Détection front descendant sur RA4
    if (RA4_bit == 0 && etat_precedent == 1) {
        Delay_ms(50);  // Anti-rebond
        
        if (RA4_bit == 0) {  // Bouton toujours appuyé
            compteur_appuis_RA4++;
            
            // Attendre relâchement
            while(RA4_bit == 0);
            Delay_ms(50);
            
            // Après 3 appuis
            if (compteur_appuis_RA4 >= 3) {
                affichage_seuils_active = 1;  // Activer affichage seuils
                compteur_appuis_RA4 = 0;      // Réinitialiser compteur
                compteur_2s = 0;              // Réinitialiser compteur 2s
                
                // Afficher les seuils immédiatement
                AfficherSeuilsSysteme();
            }
        }
    }
    
    // Sauvegarder état pour détection front
    etat_precedent = RA4_bit;
}

// ==================================================
//   FONCTIONS DE CONTROLE AQUARIUM (ANCIENNE LOGIQUE)
// ==================================================

// 1. Contrôle température (ANCIENNE LOGIQUE)
void ControlerTemperature(unsigned int temperature) {
    static unsigned char dernier_etat = 0;  // 0: normal, 1: ventilation, 2: chauffage
    
    if (temperature > SEUIL_TEMP_HAUT) {
        VENTILATEUR = 1;        // Le ventilateur s'active
        CHAUFFAGE = 0;          // Désactiver chauffage
        
        if (dernier_etat != 1) {
            AfficherMessageTemp("ventilation ON", 2000);
            dernier_etat = 1;
        }
    }
    else if (temperature < SEUIL_TEMP_BAS) {
        VENTILATEUR = 0;        // Le ventilateur s'arrête
        CHAUFFAGE = 1;          // Activer chauffage
        
        if (dernier_etat != 2) {
            AfficherMessageTemp("ventilation OFF", 2000);
            dernier_etat = 2;
        }
    }
    else {
        VENTILATEUR = 0;        // Éteindre ventilateur
        CHAUFFAGE = 0;          // Éteindre chauffage
        dernier_etat = 0;
    }
}

// 3. Gestion niveau eau (ANCIENNE LOGIQUE)
void ControlerNiveauEau(unsigned int niveau) {
    static unsigned char dernier_etat = 0;  // 0: normal, 1: pompe, 2: alerte
    
    if (niveau < SEUIL_NIVEAU_CRITIQUE) {  // Niveau critique (20%)
        POMPE_AQUARIUM = 1;  // La pompe s'active
        BUZZER = 1;          // Le buzzer s'active
        
        if (dernier_etat != 2) {
            AfficherMessageTemp("Alerte", 2000);
            dernier_etat = 2;
        }
    }
    else if (niveau < SEUIL_NIVEAU_BAS) {  // Niveau bas (50%)
        POMPE_AQUARIUM = 1;  // La pompe s'active
        BUZZER = 0;          // Buzzer éteint
        
        if (dernier_etat != 1) {
            AfficherMessageTemp("pompe activee", 2000);
            dernier_etat = 1;
        }
    }
    else if (niveau >= SEUIL_NIVEAU_HAUT) {  // Niveau haut (80%)
        POMPE_AQUARIUM = 0;  // Pompe s'arrête
        BUZZER = 0;          // Buzzer éteint
        dernier_etat = 0;
    }
    else {
        // Maintenir l'état de la pompe si elle est déjà active
        if (dernier_etat == 1) {
            POMPE_AQUARIUM = 1;
        }
    }
}

// ==================================================
//   INITIALISATION INTERRUPTIONS
// ==================================================
void Init_Interruptions() {
    INTCON.GIE = 0;

    // RB0 - LDR
    TRISB0_bit = 1;
    OPTION_REG.INTEDG = 1;
    INTCON.INTE = 1;
    INTCON.INTF = 0;

    // RB4 / RB5 / RB7 - IOC
    TRISB4_bit = 1;
    TRISB5_bit = 1;
    TRISB7_bit = 1;
    INTCON.RBIE = 1;
    INTCON.RBIF = 0;

    OPTION_REG.NOT_RBPU = 0;

    // Configurer Timer0 pour 2 secondes
    OPTION_REG = 0b00000111;  // Prescaler 1:256
    
    TMR0 = 0;                 // Initialiser Timer0
    INTCON.T0IE = 1;          // Activer interruption Timer0
    INTCON.T0IF = 0;          // Effacer drapeau

    INTCON.GIE = 1;
}

// ==================================================
//   ROUTINE D'INTERRUPTION
// ==================================================
void interrupt() {
    // ---------- RB0 - LDR ----------
    if (INTCON.INTF) {
        // Désactivé si en mode aquarium
        if (mode_maintenance == 0 && mode_actuel == 0) {
            mode_jour_nuit = LDR_DIGITAL;
            OPTION_REG.INTEDG = !OPTION_REG.INTEDG;
        }
        INTCON.INTF = 0;
    }

    // ---------- RB4 / RB5 / RB7 ----------
    if (INTCON.RBIF) {
        // Désactivé si en mode aquarium
        if (mode_actuel == 1) {
            INTCON.RBIF = 0;
            return;
        }

        // ======================
        // RB7 - MODE MAINTENANCE
        // ======================
        if (RB7_bit == 0) {
            Delay_ms(50);
            if (RB7_bit == 0) {
                mode_maintenance = !mode_maintenance;

                if (mode_maintenance == 1) {
                    // Tout éteindre
                    LED_TEMOIN = 0;
                    POMPE_CONTROLE = 0;

                    // Réinitialiser les modes
                    mode_eclairage_manuel = 0;
                    mode_pompe_manuel = 0;
                    etat_eclairage = 0;
                    etat_pompe = 0;
                }
            }
        }

        // ======================
        // Si maintenance active - ignorer RB4/RB5
        // ======================
        if (mode_maintenance == 0) {

            // RB4 - éclairage manuel
            if (RB4_bit == 0) {
                Delay_ms(50);
                if (RB4_bit == 0) {
                    mode_eclairage_manuel = 1;
                    etat_eclairage = !etat_eclairage;
                    LED_TEMOIN = etat_eclairage;
                }
            }

            // RB5 - pompe
            if (RB5_bit == 0) {
                Delay_ms(50);
                if (RB5_bit == 0) {
                    mode_pompe_manuel = 1;
                    etat_pompe = !etat_pompe;
                    POMPE_CONTROLE = etat_pompe;
                }
            }
        }

        INTCON.RBIF = 0;
    }

    // ---------- TIMER0 INTERRUPTION ----------
    if (INTCON.T0IF) {
        // Si l'affichage des seuils est activé
        if (affichage_seuils_active) {
            compteur_2s++;
            
            // Pour 4MHz avec prescaler 1:256:
            // Overflow toutes les 256 * 256µs = 65.536ms
            // Pour 2 secondes: 2000ms / 65.536ms ≈ 30.5 overflows
            if (compteur_2s >= 31) {  // Environ 2 secondes
                affichage_seuils_active = 0;  // Désactiver affichage seuils
                compteur_2s = 0;              // Réinitialiser compteur
                
                // Forcer la réinitialisation de l'affichage
                affichage_initialise = 0;
            }
        }
        
        INTCON.T0IF = 0;  // Effacer le drapeau d'interruption
    }
}

// ==================================================
//   AUTOMATISME ÉCLAIRAGE POUR MODE CONTROLE
// ==================================================
void ControlerEclairage() {
    if (mode_maintenance == 1)
        return;

    if (mode_eclairage_manuel == 1)
        return;

    if (mode_jour_nuit == 1) {   // Jour
        LED_TEMOIN = 0;
    } else {                     // Nuit
        LED_TEMOIN = 1;
    }
}

// ==================================================
//   FONCTION POUR DÉTECTER LE BOUTON RB1
// ==================================================
void Detecter_Bouton_RB1() {
    // Détection front descendant (1 -> 0)
    if (RB1_bit == 0 && bouton_precedent == 1) {
        Delay_ms(50);  // Anti-rebonds

        if (RB1_bit == 0) {  // Vérifier que le bouton est toujours pressé
            // Basculer le mode
            mode_actuel = !mode_actuel;

            // Réinitialiser l'affichage
            affichage_initialise = 0;

            // Attendre que le bouton soit relâché
            while(RB1_bit == 0) {
                Delay_ms(10);
            }
            
            // Message de confirmation
            Lcd_Cmd(_LCD_CLEAR);
            if (mode_actuel == 1) {
                Lcd_Out(1, 1, "Mode Aquarium");
                Lcd_Out(2, 1, "Active");
            } else {
                Lcd_Out(1, 1, "Mode Controle");
                Lcd_Out(2, 1, "Active");
            }
            Delay_ms(1000);
        }
    }

    // Sauvegarder l'état actuel pour la prochaine vérification
    bouton_precedent = RB1_bit;
}

// ==================================================
//   AFFICHAGE STABLE POUR MODE AQUARIUM (ANCIENNE LOGIQUE)
// ==================================================
void AfficherAquariumStable(unsigned int temp, unsigned int niveau, unsigned int heure) {
    unsigned char periode;

    // Déterminer la période (ANCIENNE LOGIQUE)
    if (heure >= HEURE_DEBUT_JOUR && heure < HEURE_FIN_JOUR) {
        periode = 1;  // Jour
    } else {
        periode = 0;  // Nuit
    }

    // N'afficher l'en-tête que la première fois ou si changement de mode
    if (affichage_initialise == 0) {
        Lcd_Cmd(_LCD_CLEAR);

        // Afficher les labels fixes
        Lcd_Out(1, 1, "T:");      // Température
        Lcd_Out(1, 6, "C");       // C après température
        Lcd_Out(1, 9, "N:");      // Niveau
        Lcd_Out(1, 14, "%");      // % après niveau

        Lcd_Out(2, 1, "H:");      // Heure
        Lcd_Out(2, 6, "h");       // h après heure
        Lcd_Out(2, 9, "Per:");    // Période

        affichage_initialise = 1;
    }

    // Mettre à jour seulement les valeurs qui changent

    // Température (position 3-4)
    if (temp != temp_prec) {
        AfficherNb(temp, 3, 1);
        temp_prec = temp;
    }

    // Niveau (position 11-12)
    if (niveau != niveau_prec) {
        AfficherNb(niveau, 11, 1);
        niveau_prec = niveau;
    }

    // Heure (position 3-4 sur ligne 2)
    if (heure != heure_prec) {
        AfficherNb(heure, 3, 2);
        heure_prec = heure;
    }

    // Période (position 14 sur ligne 2)
    if (periode != periode_prec) {
        if (periode == 1) {
            Lcd_Chr(2, 14, 'J');  // Jour
        } else {
            Lcd_Chr(2, 14, 'N');  // Nuit
        }
        periode_prec = periode;
    }
}

// ==================================================
//   GESTION DES LAMPES AQUARIUM (ANCIENNE LOGIQUE)
// ==================================================
void ControlerLampesAquarium(unsigned int heure) {
    // ANCIENNE LOGIQUE: basée sur l'heure simulée
    if (heure >= HEURE_DEBUT_JOUR && heure < HEURE_FIN_JOUR) {
        // JOUR (7h-19h) - les lampes s'éteignent
        LAMPE_1 = 0;
        LAMPE_2 = 0;
        LAMPE_3 = 0;
        LAMPE_4 = 0;
    } else {
        // NUIT (19h-7h) - les lampes s'allument
        LAMPE_1 = 1;
        LAMPE_2 = 1;
        LAMPE_3 = 1;
        LAMPE_4 = 1;
    }
}

// ==================================================
//   FONCTION POUR AFFICHER L'ÉTAT DE REPOS
// ==================================================
void AfficherEtatRepos() {
    // ---------- MODE CONTROLE ECLAIRAGE/POMPE ----------
    if (mode_actuel == 0) {
        // Si maintenance active
        if (mode_maintenance == 1) {
            Lcd_Cmd(_LCD_CLEAR);
            Lcd_Out(1, 1, "Maintenance");
            Lcd_Out(2, 1, "ACTIVE");
        } else {
            // Sinon fonctionnement normal
            ControlerEclairage();

            Lcd_Cmd(_LCD_CLEAR);

            // LIGNE 1 : Jour / Nuit
            if (mode_jour_nuit == 1) {
                Lcd_Out(1, 1, "Jour");
            } else {
                Lcd_Out(1, 1, "Nuit");
            }

            // Indicateur mode
            Lcd_Out(1, 10, "Controle");

            // LIGNE 2 : Eclairage + Pompe
            Lcd_Out(2, 1, "Ecl:");
            if (LED_TEMOIN == 1) {
                Lcd_Out(2, 6, "ON ");
            } else {
                Lcd_Out(2, 6, "OFF");
            }

            Lcd_Out(2, 11, "Pmp:");
            if (POMPE_CONTROLE == 1) {
                Lcd_Out(2, 16, "ON ");
            } else {
                Lcd_Out(2, 16, "OFF");
            }
        }
    }
    // ---------- MODE AQUARIUM (ANCIENNE LOGIQUE) ----------
    else {
        // Lecture capteurs (ANCIENNE LOGIQUE)
        unsigned int temperature_adc = ADC_Read(1);   // AN1: Température
        unsigned int temperature_c = temperature_adc / 2;

        unsigned int niveau_adc = lire_niveau_moyenne();  // AN0: Niveau
        unsigned int niveau_pourcent = niveau_adc / 10;

        unsigned int ldr_adc = ADC_Read(2);           // AN2: LDR pour heure simulée
        unsigned int heure_simulee = (ldr_adc * 24) / 1023;

        // Appeler les fonctions de contrôle aquarium (ANCIENNE LOGIQUE)
        ControlerTemperature(temperature_c);
        ControlerNiveauEau(niveau_pourcent);
        
        // Contrôle des lampes basé sur l'heure simulée (ANCIENNE LOGIQUE)
        ControlerLampesAquarium(heure_simulee);

        // Afficher avec mise à jour partielle pour éviter le clignotement
        AfficherAquariumStable(temperature_c, niveau_pourcent, heure_simulee);
    }
}

// ==================================================
//   PROGRAMME PRINCIPAL
// ==================================================
void main() {
    // Configuration des ports
    TRISD = 0x00;
    TRISC = 0x00;  // Toutes les broches PORTC en sortie
    TRISA = 0xFF;  // PORTA en entrée (RA4 comme entrée)
    TRISB1_bit = 1;  // RB1 en entrée

    // Activer pull-up interne sur PORTB
    OPTION_REG = 0b01111111;

    // Configuration ADC
    ADCON1 = 0x80;  // Tous analogiques
    CMCON = 0x07;

    // État initial - tout éteint
    PORTC = 0x00;

    // Initialisation LCD
    Delay_ms(100);
    Lcd_Init();
    Lcd_Cmd(_LCD_CLEAR);
    Lcd_Cmd(_LCD_CURSOR_OFF);

    // Message initial
    Lcd_Out(1, 1, "SYSTEME CONTROLE");
    Lcd_Out(2, 1, "RB1:Aquarium");
    Delay_ms(1000);

    // Initialisation interruptions
    Init_Interruptions();

    while(1) {
        // ---------- Détecter le bouton RA4 pour afficher les seuils ----------
        GererAppuisRA4();

        // ---------- Détecter le bouton RB1 pour changer de mode ----------
        Detecter_Bouton_RB1();

        // ---------- Si affichage des seuils actif ----------
        if (affichage_seuils_active) {
            // Afficher les seuils (déjà affiché dans GererAppuisRA4)
            // La temporisation de 2 secondes est gérée dans l'interruption Timer0
            // On ne fait rien ici, on attend que le timer termine
        } else {
            // ---------- ÉTAT DE REPOS NORMAL ----------
            AfficherEtatRepos();
        }

        // Délai pour la boucle principale
        Delay_ms(200);
    }
}
