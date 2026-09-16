#include "MatCalibration.h"

// Initialise les dimensions physiques du tapis, exprimées en centimètres.
MatCalibration::MatCalibration(float lengthCm,float widthCm)
    : matLengthCm(lengthCm),
      matWidthCm(widthCm) {
}

// Ajoute un coin détecté dans l'image.
// Ordre attendu : haut-gauche, haut-droite, bas-droite, bas-gauche.
void MatCalibration::addCorner(const cv::Point2f& point) {
    // Quatre points suffisent pour calculer une transformation perspective.
    if (corners.size() < 4) {
        corners.push_back(point);
    }
}

// Efface les points existants pour permettre une nouvelle calibration.
void MatCalibration::reset() {
    corners.clear();
}

// Indique si les quatre coins nécessaires ont été enregistrés.
bool MatCalibration::isCalibrated() const {
    return corners.size() == 4;
}

// Renvoie une référence en lecture seule afin d'éviter une copie.
const std::vector<cv::Point2f>&
MatCalibration::getCorners() const {
    return corners;
}

// Calcule la matrice permettant de redresser la vue du tapis et de la
// projeter dans une image rectangulaire aux dimensions demandées.
cv::Mat MatCalibration::computeHomography(
    int outputWidth,
    int outputHeight) {

    // Le calcul est impossible tant que les quatre coins ne sont pas connus.
    if (!isCalibrated()) {
        return cv::Mat();
    }

    // Coins correspondants dans l'image de destination. Ils suivent le même
    // ordre que les points enregistrés avec addCorner().
    std::vector<cv::Point2f> destination = {
        {0.0f, 0.0f},
        {(float)outputWidth, 0.0f},
        {(float)outputWidth, (float)outputHeight},
        {0.0f, (float)outputHeight}
    };

    // Produit la matrice de transformation perspective 3 x 3.
    return cv::getPerspectiveTransform(
        corners,
        destination
    );
}
