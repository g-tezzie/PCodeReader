#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/videoio.hpp>
#include <iostream>

#include <string>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/regex.hpp>

using namespace cv;
using namespace std;

class Flipflop {
    bool qint;
public:
    Flipflop() {
        qint = false;
    }
    bool q(void) {
        return qint;
    }
    void flip(void) {
        qint = !qint;
    }
};

const struct {
    int width = 400;
    int height = 100;
} Viewport;

int main(int argc, char **argv) {
    const auto regex = boost::basic_regex<char>("[^A-Z]");
    auto flip = Flipflop();
    //--- INITIALIZE VIDEOCAPTURE
    VideoCapture cap;
    tesseract::TessBaseAPI *tesseract = new tesseract::TessBaseAPI();
    tesseract->Init(NULL, "eng", tesseract::OEM_DEFAULT);
    tesseract->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK); // SPARSE_TEXT_OSD は多すぎた

    // open the default camera using default API
    // cap.open(0);
    // OR advance usage: select any API backend
    int deviceID = 1;             // 0 = open default camera
    int apiID = cv::CAP_ANY;      // 0 = autodetect default API
    // open selected camera using selected API
    cap.open(deviceID, apiID);
    // check if we succeeded
    if (!cap.isOpened()) {
        cerr << "ERROR! Unable to open camera\n";
        return -1;
    }

    //--- QR
    QRCodeDetector qrDet;
    Mat qrPos;

    //--- GRAB AND WRITE LOOP
    cout << "Start grabbing" << endl << "Press any key to terminate" << endl;
    Mat qrPos32s;
    Mat frame;
    Mat im; // = frame;

    for (;;) {
        // wait for a new frame from camera and store it into 'frame'
        cap.read(frame);
        // check if we succeeded
        if (frame.empty()) {
            cerr << "ERROR! blank frame grabbed\n";
            break;
        }

//		std::string detectAndDecode = //
//				qrDet.detectAndDecode(frame, qrPos, noArray());
        const bool det = qrDet.detect(frame, qrPos);
        if (det) {
            qrPos.convertTo(qrPos32s, CV_32S);
            std::string detectAndDecode = qrDet.decode(frame, qrPos32s);
            {
                static string latched = "";
                if (latched.compare(detectAndDecode)
                        && 4 < detectAndDecode.length()) {
                    static int lines = 0;
                    lines++;
                    latched = detectAndDecode;
                    cout << latched.c_str() << endl;
                    if (lines % 3 == 0)
                        cout << endl;
                }
            }
        }

        {

            string outText;

            Mat warpGround = ( //
                    Mat_<float>(2, 3) << 1, 0, -(320 - Viewport.width / 2), //
                    /*                 */0, 1, -(240 - Viewport.height / 2));
            warpAffine(frame, im, warpGround, Size(Viewport.width, Viewport.height));
//            frame.copyTo(im);

            tesseract->SetImage(im.data, im.cols, im.rows, 3, im.step);
            tesseract->SetSourceResolution(250);

            outText = string(tesseract->GetUTF8Text());
            boost::algorithm::trim(outText);
            boost::algorithm::erase_all_regex(outText, regex);
            auto length = outText.length();
            if (length == 12)
                cout << outText << ", " << length << endl;

        }

        { // write target
            const int r[] = { //
                    320 - Viewport.width / 2, 240 - Viewport.height / 2, //
                    320 + Viewport.width / 2, 240 + Viewport.height / 2 };
            const int v[/* 4] [2 */] = { r[0], r[1], //
                    r[0], r[3], //
                    r[2], r[3], //
                    r[2], r[1] //
                    };
            const Matx<int, 4, 2> viewport(v);
//		const Matx<float, 4, 2> viewport = {{10,10}, {10,20}, {20,20}, {20,10}};
            polylines(frame, viewport, true, Scalar(127, 191, 127), 3, LINE_AA);
            if (det) {
                polylines(frame, qrPos32s, true, Scalar(127, 127, 223), 3,
                        LINE_AA);
            }
        }
        // show live and wait for a key with timeout long enough to show images
        std::string live = "Live";
        imshow(live, flip.q() ? im : frame);
        int keycode = waitKey(5);

        if (keycode == 32)
            flip.flip();
        else if (keycode >= 0) {
            cerr << "keycode: " << keycode << "; then exit." << endl;
            break;
        }
    }
    tesseract->End();
    // the camera will be deinitialized automatically in VideoCapture destructor
    return 0;
}
