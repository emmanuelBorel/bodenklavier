#include <OpenNI.h>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

#include "MatCalibration.h"
#include "PianoLayout.h"
#include "TouchDetector.h"
#include "KeyStateManager.h"
#include "AudioEngine.h"
#include "ConflictResolver.h"
#include "NoteMapper.h"

using namespace openni;
using namespace std;
using namespace cv;


// ============================================================
// Objets globaux
// ============================================================

MatCalibration calibration;
TouchDetector touchDetector;
ConflictResolver conflictResolver;
NoteMapper noteMapper;


// ============================================================
// Derniere image Depth rectifiee
//
// Elle est conservee ici afin que la touche C puisse enregistrer
// le tapis vide comme profondeur de reference.
// ============================================================

Mat latestRectifiedDepth;


// ============================================================
// Callback souris pour la calibration
// ============================================================

void mouseCallback(
    int event,
    int x,
    int y,
    int flags,
    void* userdata)
{
    if (event == EVENT_LBUTTONDOWN)
    {
        if (!calibration.isCalibrated())
        {
            calibration.addCorner(
                Point2f(
                    static_cast<float>(x),
                    static_cast<float>(y)
                )
            );

            cout
                << "Point de calibration ajoute : "
                << x
                << ", "
                << y
                << endl;
        }
    }
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // ========================================================
    // 1. INITIALISATION OPENNI
    // ========================================================

    if (OpenNI::initialize() != STATUS_OK)
    {
        cerr
            << "Erreur initialisation OpenNI : "
            << OpenNI::getExtendedError()
            << endl;

        return 1;
    }


    Device device;


    if (device.open(ANY_DEVICE) != STATUS_OK)
    {
        cerr
            << "Erreur ouverture camera : "
            << OpenNI::getExtendedError()
            << endl;

        OpenNI::shutdown();

        return 1;
    }


    // ========================================================
    // 2. FLUX DEPTH
    // ========================================================

    VideoStream depthStream;


    if (
        depthStream.create(
            device,
            SENSOR_DEPTH
        ) != STATUS_OK
    )
    {
        cerr
            << "Erreur creation depth stream."
            << endl;

        device.close();
        OpenNI::shutdown();

        return 1;
    }


    if (depthStream.start() != STATUS_OK)
    {
        cerr
            << "Erreur demarrage depth stream."
            << endl;

        depthStream.destroy();

        device.close();

        OpenNI::shutdown();

        return 1;
    }


    // ========================================================
    // 3. FLUX RGB
    // ========================================================

    VideoStream colorStream;

    bool colorAvailable = false;


    if (device.hasSensor(SENSOR_COLOR))
    {
        if (
            colorStream.create(
                device,
                SENSOR_COLOR
            ) == STATUS_OK
        )
        {
            if (
                colorStream.start()
                == STATUS_OK
            )
            {
                colorAvailable = true;

                cout
                    << "Flux couleur demarre."
                    << endl;
            }
            else
            {
                cerr
                    << "Impossible de demarrer "
                    << "le flux couleur."
                    << endl;
            }
        }
        else
        {
            cerr
                << "Impossible de creer "
                << "le flux couleur."
                << endl;
        }
    }
    else
    {
        cerr
            << "Aucun capteur couleur detecte."
            << endl;
    }


    // ========================================================
    // 4. REGISTRATION DEPTH -> RGB
    // ========================================================

    bool registrationEnabled = false;


    if (colorAvailable)
    {
        if (
            device.isImageRegistrationModeSupported(
                IMAGE_REGISTRATION_DEPTH_TO_COLOR
            )
        )
        {
            Status rc =
                device.setImageRegistrationMode(
                    IMAGE_REGISTRATION_DEPTH_TO_COLOR
                );


            if (rc == STATUS_OK)
            {
                registrationEnabled = true;

                cout
                    << "Image registration "
                    << "DEPTH_TO_COLOR activee."
                    << endl;
            }
            else
            {
                cerr
                    << "Echec activation image registration : "
                    << OpenNI::getExtendedError()
                    << endl;
            }
        }
        else
        {
            cerr
                << "IMAGE_REGISTRATION_DEPTH_TO_COLOR "
                << "non supporte."
                << endl;
        }
    }


    // ========================================================
    // 5. FENETRES
    // ========================================================

    const string colorWindow =
        "Bodenklavier - Calibration RGB";

    const string depthWindow =
        "Bodenklavier - Depth";

    const string rectifiedWindow =
        "Bodenklavier - Tapis rectifie";


    if (colorAvailable)
    {
        namedWindow(
            colorWindow,
            WINDOW_AUTOSIZE
        );


        setMouseCallback(
            colorWindow,
            mouseCallback,
            nullptr
        );
    }


    namedWindow(
        depthWindow,
        WINDOW_AUTOSIZE
    );


    // ========================================================
    // 6. INFORMATIONS TERMINAL
    // ========================================================

    cout << endl;

    cout
        << "=========================================="
        << endl;

    cout
        << "        BODENKLAVIER - SYSTEME 3D         "
        << endl;

    cout
        << "=========================================="
        << endl;

    cout << endl;


    if (colorAvailable)
    {
        cout
            << "Clique sur les 4 coins du tapis "
            << "dans la fenetre RGB :"
            << endl;

        cout
            << "1. Coin superieur gauche"
            << endl;

        cout
            << "2. Coin superieur droit"
            << endl;

        cout
            << "3. Coin inferieur droit"
            << endl;

        cout
            << "4. Coin inferieur gauche"
            << endl;
    }


    cout << endl;


    cout
        << "Registration : "
        << (
            registrationEnabled
                ? "ON"
                : "OFF"
        )
        << endl;


    cout << endl;

    cout
        << "Commandes :"
        << endl;

    cout
        << "C = capturer le tapis vide "
        << "comme reference Depth"
        << endl;

    cout
        << "R = recommencer la calibration"
        << endl;

    cout
        << "Q = quitter"
        << endl;

    cout << endl;


    // ========================================================
    // 7. FRAMES
    // ========================================================

    VideoFrameRef depthFrame;
    VideoFrameRef colorFrame;


    // ========================================================
    // 8. BOUCLE PRINCIPALE
    // ========================================================

    KeyStateManager* keyStateManager =
        nullptr;


   AudioEngine audioEngine;

if (
    !audioEngine.initialize(
        "media/sounds_piano"
    )
)
{
    cerr
        << "Audio non disponible."
        << endl;
}
else
{
    const vector<string> whiteAudioNotes = {
        "C4",
        "D4",
        "E4",
        "F4",
        "G4",
        "A4",
        "B4",

        "C5",
        "D5",
        "E5",
        "F5",
        "G5",
        "A5",
        "B5"
    };

    for (
        const string& note :
        whiteAudioNotes
    )
    {
        audioEngine.loadSound(
            note,
            note + ".wav"
        );
    }

    const vector<string> blackAudioNotes = {
        "Cs4",
        "Ds4",
        "Fs4",
        "Gs4",
        "As4",

        "Cs5",
        "Ds5",
        "Fs5",
        "Gs5",
        "As5"
    };

    for (
        const string& note :
        blackAudioNotes
    )
    {
        audioEngine.loadSound(
            note,
            note + ".wav"
        );
    }
}


    while (true)
    {
        // ====================================================
        // DEPTH
        // ====================================================

        if (
            depthStream.readFrame(
                &depthFrame
            ) != STATUS_OK
            ||
            !depthFrame.isValid()
        )
        {
            continue;
        }


        const int depthWidth =
            depthFrame.getWidth();

        const int depthHeight =
            depthFrame.getHeight();


        const DepthPixel* depthData =
            static_cast<const DepthPixel*>(
                depthFrame.getData()
            );


        Mat depth16(
            depthHeight,
            depthWidth,
            CV_16U,
            const_cast<DepthPixel*>(
                depthData
            )
        );


        // ====================================================
        // VISUALISATION DEPTH
        // ====================================================

        Mat depth8;


        depth16.convertTo(
            depth8,
            CV_8U,
            255.0 / 4000.0
        );


        Mat depthColored;


        applyColorMap(
            depth8,
            depthColored,
            COLORMAP_JET
        );


        putText(
            depthColored,

            registrationEnabled
                ? "Registration: ON"
                : "Registration: OFF",

            Point(20, 40),

            FONT_HERSHEY_SIMPLEX,

            0.7,

            Scalar(
                255,
                255,
                255
            ),

            2
        );


        if (touchDetector.hasReference())
        {
            putText(
                depthColored,

                "Reference Depth: OK",

                Point(20, 70),

                FONT_HERSHEY_SIMPLEX,

                0.7,

                Scalar(
                    0,
                    255,
                    0
                ),

                2
            );
        }
        else
        {
            putText(
                depthColored,

                "Reference Depth: NON",

                Point(20, 70),

                FONT_HERSHEY_SIMPLEX,

                0.7,

                Scalar(
                    0,
                    255,
                    255
                ),

                2
            );
        }


        imshow(
            depthWindow,
            depthColored
        );


        // ====================================================
        // RGB
        // ====================================================

        if (colorAvailable)
        {
            if (
                colorStream.readFrame(
                    &colorFrame
                ) == STATUS_OK
                &&
                colorFrame.isValid()
            )
            {
                const int colorWidth =
                    colorFrame.getWidth();

                const int colorHeight =
                    colorFrame.getHeight();


                const RGB888Pixel* colorData =
                    static_cast<
                        const RGB888Pixel*
                    >(
                        colorFrame.getData()
                    );


                Mat rgb(
                    colorHeight,
                    colorWidth,
                    CV_8UC3,
                    const_cast<RGB888Pixel*>(
                        colorData
                    )
                );


                Mat bgr;


                cvtColor(
                    rgb,
                    bgr,
                    COLOR_RGB2BGR
                );


                // ============================================
                // Image propre sans annotations
                // ============================================

                Mat cleanBgr =
                    bgr.clone();


                // ============================================
                // POINTS DE CALIBRATION
                // ============================================

                const auto& corners =
                    calibration.getCorners();


                for (
                    size_t i = 0;
                    i < corners.size();
                    ++i
                )
                {
                    circle(
                        bgr,
                        corners[i],
                        8,
                        Scalar(
                            0,
                            0,
                            255
                        ),
                        -1
                    );


                    putText(
                        bgr,

                        to_string(
                            i + 1
                        ),

                        corners[i]
                            +
                        Point2f(
                            10,
                            -10
                        ),

                        FONT_HERSHEY_SIMPLEX,

                        0.8,

                        Scalar(
                            0,
                            0,
                            255
                        ),

                        2
                    );
                }


                // ============================================
                // CALIBRATION COMPLETE
                // ============================================

                if (corners.size() == 4)
                {
                    // ========================================
                    // Dessiner contour tapis
                    // ========================================

                    for (
                        int i = 0;
                        i < 4;
                        ++i
                    )
                    {
                        line(
                            bgr,

                            corners[i],

                            corners[
                                (i + 1) % 4
                            ],

                            Scalar(
                                0,
                                255,
                                255
                            ),

                            2
                        );
                    }


                    putText(
                        bgr,

                        "CALIBRATION OK",

                        Point(20, 40),

                        FONT_HERSHEY_SIMPLEX,

                        1.0,

                        Scalar(
                            0,
                            255,
                            0
                        ),

                        2
                    );


                    // ========================================
                    // DIMENSIONS RECTIFIEES DU TAPIS
                    //
                    // 255 cm x 80 cm
                    // environ 5 pixels / cm
                    // ========================================

                    const int rectifiedWidth =
                        1275;

                    const int rectifiedHeight =
                        400;


                    // ========================================
                    // HOMOGRAPHIE
                    // ========================================

                    Mat H =
                        calibration.computeHomography(
                            rectifiedWidth,
                            rectifiedHeight
                        );


                    if (!H.empty())
                    {
                        // ====================================
                        // RECTIFICATION RGB
                        // ====================================

                        Mat rectifiedMat;


                        warpPerspective(
                            cleanBgr,

                            rectifiedMat,

                            H,

                            Size(
                                rectifiedWidth,
                                rectifiedHeight
                            )
                        );


                        // ====================================
                        // RECTIFICATION DEPTH
                        //
                        // INTER_NEAREST est important pour
                        // ne pas interpoler les valeurs Depth.
                        // ====================================

                        Mat rectifiedDepth;


                        warpPerspective(
                            depth16,

                            rectifiedDepth,

                            H,

                            Size(
                                rectifiedWidth,
                                rectifiedHeight
                            ),

                            INTER_NEAREST
                        );


                        // ====================================
                        // Sauvegarder la dernière image Depth
                        // rectifiée.
                        // ====================================

                        rectifiedDepth.copyTo(
                            latestRectifiedDepth
                        );


                        // ====================================
                        // GENERATION DES 24 TOUCHES
                        // ====================================

                        PianoLayout pianoLayout(
                            rectifiedWidth,
                            rectifiedHeight
                        );


                        const vector<PianoKey>& pianoKeys =
                            pianoLayout.getKeys();


                        if (keyStateManager == nullptr)
                        {
                            keyStateManager =
                                new KeyStateManager(
                                    pianoKeys
                                );

                            cout
                                << "KeyStateManager initialise."
                                << endl;
                        }


                        // ====================================
                        // DETECTION DES TOUCHES
                        // ====================================

                        vector<int> detectedKeys;


                        if (
                            touchDetector.hasReference()
                        )
                        {
                            detectedKeys =
                                touchDetector
                                .detectTouchedKeys(
                                    rectifiedDepth,
                                    pianoKeys
                                );
                        }


                        // ====================================
                        // RESOLUTION DES CONFLITS SPATIAUX
                        // ====================================

                        vector<int> touchedKeys =
                            conflictResolver.resolve(
                                detectedKeys,
                                pianoKeys
                            );


                        // ====================================
                        // EVENEMENTS DES TOUCHES
                        // ====================================

                        vector<KeyEvent> keyEvents;


                        if (keyStateManager != nullptr)
                        {
                            keyEvents =
                                keyStateManager->update(
                                    touchedKeys
                                );
                        }


                        for (
                            const KeyEvent& event :
                            keyEvents
                        )
                        {
                            if (
                                event.type ==
                                KeyEventType::PRESS
                            )
                            {
                                cout
                                    << "PRESS : "
                                    << event.note
                                    << endl;
                                //convertir l'identifiant physique en note musicale
                                string musicalNote =
                                noteMapper.toMusicalNote(
                                    event.note
                                );
                                audioEngine.play(
                                    musicalNote
                                );
                            }
                            else if (
                                event.type ==
                                KeyEventType::RELEASE
                            )
                            {
                                cout
                                    << "RELEASE : "
                                    << event.note
                                    << endl;
                                //utlisliser la meme correspondance pour arreter le son
                                string musicalNote = 
                                noteMapper.toMusicalNote(
                                    event.note
                                );

                                audioEngine.stop(
                                    musicalNote
                                );
                            }
                        }


                        // ====================================
                        // Dessiner les zones normales
                        // ====================================

                        pianoLayout.draw(
                            rectifiedMat
                        );


                        // ====================================
                        // Dessiner les touches detectees
                        // en vert
                        // ====================================

                        for (
                            int index
                            :
                            touchedKeys
                        )
                        {
                            if (
                                index >= 0
                                &&
                                index
                                    <
                                static_cast<int>(
                                    pianoKeys.size()
                                )
                            )
                            {
                                rectangle(
                                    rectifiedMat,

                                    pianoKeys[
                                        index
                                    ].zone,

                                    Scalar(
                                        0,
                                        255,
                                        0
                                    ),

                                    4
                                );
                            }
                        }


                        // ====================================
                        // Afficher statut de la référence
                        // ====================================

                        if (
                            touchDetector.hasReference()
                        )
                        {
                            putText(
                                rectifiedMat,

                                "REFERENCE DEPTH OK",

                                Point(
                                    20,
                                    30
                                ),

                                FONT_HERSHEY_SIMPLEX,

                                0.7,

                                Scalar(
                                    0,
                                    255,
                                    0
                                ),

                                2
                            );
                        }
                        else
                        {
                            putText(
                                rectifiedMat,

                                "APPUYER SUR C : TAPIS VIDE",

                                Point(
                                    20,
                                    30
                                ),

                                FONT_HERSHEY_SIMPLEX,

                                0.7,

                                Scalar(
                                    0,
                                    255,
                                    255
                                ),

                                2
                            );
                        }


                        // ====================================
                        // Affichage tapis rectifie
                        // ====================================

                        imshow(
                            rectifiedWindow,
                            rectifiedMat
                        );
                    }
                }
                else
                {
                    // ========================================
                    // Calibration pas encore complete
                    // ========================================

                    string message =
                        "Coin suivant : "
                        +
                        to_string(
                            corners.size()
                            + 1
                        )
                        +
                        "/4";


                    putText(
                        bgr,

                        message,

                        Point(
                            20,
                            40
                        ),

                        FONT_HERSHEY_SIMPLEX,

                        0.8,

                        Scalar(
                            0,
                            255,
                            255
                        ),

                        2
                    );
                }


                // ============================================
                // Affichage RGB
                // ============================================

                imshow(
                    colorWindow,
                    bgr
                );
            }
        }


        // ====================================================
        // CLAVIER
        // ====================================================

        char c =
            static_cast<char>(
                waitKey(1)
            );


        // ====================================================
        // Q = QUITTER
        // ====================================================

        if (
            c == 'q'
            ||
            c == 'Q'
        )
        {
            break;
        }


        // ====================================================
        // R = RESET CALIBRATION
        // ====================================================

        if (
            c == 'r'
            ||
            c == 'R'
        )
        {
            calibration.reset();


            if (keyStateManager != nullptr)
            {
                delete keyStateManager;
                keyStateManager = nullptr;
            }


            latestRectifiedDepth.release();


            destroyWindow(
                rectifiedWindow
            );


            cout << endl;

            cout
                << "Calibration reinitialisee."
                << endl;

            cout
                << "Clique a nouveau sur "
                << "les 4 coins du tapis."
                << endl;
        }


        // ====================================================
        // C = CAPTURE REFERENCE DEPTH
        // ====================================================

        if (
            c == 'c'
            ||
            c == 'C'
        )
        {
            if (
                calibration.isCalibrated()
                &&
                !latestRectifiedDepth.empty()
            )
            {
                touchDetector.setReferenceDepth(
                    latestRectifiedDepth
                );


                cout << endl;

                cout
                    << "===================================="
                    << endl;

                cout
                    << "REFERENCE DEPTH ENREGISTREE"
                    << endl;

                cout
                    << "Le tapis doit rester fixe."
                    << endl;

                cout
                    << "Tu peux maintenant poser "
                    << "un pied sur une touche."
                    << endl;

                cout
                    << "===================================="
                    << endl;

                cout << endl;
            }
            else
            {
                cout
                    << "Impossible de capturer "
                    << "la reference Depth."
                    << endl;

                cout
                    << "Effectue d'abord la calibration "
                    << "des 4 coins."
                    << endl;
            }
        }


        // ====================================================
        // Petit délai
        // ====================================================

        usleep(
            30000
        );
    }


    // ========================================================
    // 9. NETTOYAGE
    // ========================================================

    if (colorAvailable)
    {
        colorStream.stop();
        colorStream.destroy();
    }


    depthStream.stop();
    depthStream.destroy();


    device.close();


    OpenNI::shutdown();


    destroyAllWindows();


    if (keyStateManager != nullptr)
    {
        delete keyStateManager;
        keyStateManager = nullptr;
    }


    return 0;
}
