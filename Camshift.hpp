#include "opencv2/core/utility.hpp"
#include "opencv2/core/ocl.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"

#include <iostream>
#include <cctype>

using namespace std;
using namespace cv;

static UMat image;
static bool backprojMode = false;
static bool selectObject = false;
static int trackObject = 0;
static bool showHist = true;
static Rect selection;
static int vmin = 10, vmax = 256, smin = 35;
Mat img;
time_t t;
char tmp[64];

Rect trackWindow;
int hsize = 16;
float hranges[2] = { 0, 180 };

//const char * const keys = { "{@camera_number| 0 | camera number}" };
//CommandLineParser parser(argc, argv, keys);
//int camNum = parser.get<int>(0);

Mat histimg(200, 320, CV_8UC3, Scalar::all(0));     //統計圖Mat
UMat hsv, hist, hue, mask, backproj;
bool paused = false;

//滑鼠事件
static void onMouse(int event, int x, int y, int, void*)
{
    static Point origin;

    if (selectObject)
    {
        selection.x = min(x, origin.x);
        selection.y = min(y, origin.y);
        selection.width = abs(x - origin.x);
        selection.height = abs(y - origin.y);

        selection &= Rect(0, 0, image.cols, image.rows);
    }

    //按下左鍵按下及放開的事件內容
    switch (event)
    {
    case EVENT_LBUTTONDOWN://左鍵按下
        origin = Point(x, y);
        selection = Rect(x, y, 0, 0);
        selectObject = true; //用來計算x,y,width,height的flag
        break;
    case EVENT_LBUTTONUP://左鍵放開
        selectObject = false;
        if (selection.width > 0 && selection.height > 0)
            trackObject = -1;
        break;
    default:
        break;
    }
}

static void help()
{
    cout << "\nThis is a demo that shows mean-shift based tracking using Transparent API\n"
            "You select a color objects such as your face and it tracks it.\n"
            "This reads from video camera (0 by default, or the camera number the user enters\n"
            "Usage: \n"
            "   ./camshiftdemo [camera number]\n";

    cout << "\n\nHot keys: \n"
            "\tESC - quit the program\n"
            "\ts - stop the tracking\n"
            "\tb - switch to/from backprojection view\n"
            "\th - show/hide object histogram\n"
            "\tp - pause video\n"
            "\tc - use OpenCL or not\n"
            "To initialize tracking, select the object with mouse\n";
}

//鍵盤事件 要跑迴圈來執行
void Keyfunction(char c)
{
        switch(c)
        {
        case 's'://將圖片存檔下來 檔案名稱包含日期時間
        {
            t = time(0);
            strftime( tmp, sizeof(tmp), "%Y%m%d_%X",localtime(&t) );
            char path[] = "Test_Image";
            strcat(path, tmp);
            strcat(path, ".png");
            cout << "path = " << path << endl;
             imwrite( path, image );
            break;
        }
        case 'b'://使用/不使用bacproject模式
            backprojMode = !backprojMode;
            break;
        case 't'://清除已選區域
            trackObject = 0;
            histimg = Scalar::all(0);
            break;
        case 'h'://顯示/部顯示統計圖
            showHist = !showHist;
            if (!showHist)
                destroyWindow("Histogram");
            else
                namedWindow("Histogram", WINDOW_AUTOSIZE);
            break;
        case 'p'://迴圈暫停按鈕
            paused = !paused;
            break;
        case 'c'://使用openCL加速
            ocl::setUseOpenCL(!ocl::useOpenCL());
        default:
            break;
        }
}

//原程式碼 可以框選區域
void camshift(Mat img)
{
    help();

    namedWindow("Histogram", WINDOW_NORMAL);
    namedWindow("CamShift Demo", WINDOW_NORMAL);
    setMouseCallback("CamShift Demo", onMouse);
    createTrackbar("Vmin", "CamShift Demo", &vmin, 256);
    createTrackbar("Vmax", "CamShift Demo", &vmax, 256);
    createTrackbar("Smin", "CamShift Demo", &smin, 256);

    while(true)
    {
    img.copyTo(image);

    if (!paused)
    {
        cvtColor(image, hsv, COLOR_BGR2HSV);

        if (trackObject)
        {
            int _vmin = vmin, _vmax = vmax;

            inRange(hsv, Scalar(0, smin, min(_vmin, _vmax)),
                    Scalar(180, 256, max(_vmin, _vmax)), mask);

            int fromTo[2] = { 0,0 };
            hue.create(hsv.size(), hsv.depth());
            mixChannels(vector<UMat>(1, hsv), vector<UMat>(1, hue), fromTo, 1);

            cout<<"trackObject = "<<trackObject<<endl;
            if (trackObject < 0)
            {
                UMat roi(hue, selection), maskroi(mask, selection);
                calcHist(vector<Mat>(1, roi.getMat(ACCESS_READ)), vector<int>(1, 0),
                             maskroi, hist, vector<int>(1, hsize), vector<float>(hranges, hranges + 2));
                normalize(hist, hist, 0, 255, NORM_MINMAX);

                Mat _temp = hist.getMat(ACCESS_RW);
                for(int i = 0; i < hsize; i++)
                    cout << "_temp.at<float>(" << i <<") =" << _temp.at<float>(i) << endl;

                for(int i = 0; i < hsize; i++)
                    _temp.at<float>(i) = 0;
                _temp.at<float>(9) = 255;
                _temp.at<float>(15) = 255;

                for(int i = 0; i < hsize; i++)
                    cout << "after _temp.at<float>(" << i <<") =" << _temp.at<float>(i) << endl;
               //_temp.at<float>(9)*histimg.rows/255 = 255;
               // _temp.at<float>(15)*histimg.rows/255 = 255;

                trackWindow = selection;
                trackObject = 1;

                //畫統計圖
                histimg = Scalar::all(0);
                int binW = histimg.cols / hsize;//一條顏色的寬度
                Mat buf (1, hsize, CV_8UC3);
                for (int i = 0; i < hsize; i++)
                    buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180./hsize), 255, 255);
                cvtColor(buf, buf, COLOR_HSV2BGR);

                Mat _hist = hist.getMat(ACCESS_READ);
                for (int i = 0; i < hsize; i++)
                {
                    int val = saturate_cast<int>(_hist.at<float>(i)*histimg.rows/255);
                    rectangle(histimg,
                               Point(i*binW, histimg.rows),
                               Point((i+1)*binW, histimg.rows - val),  // -val 是為了讓數值調往上長
                               Scalar(buf.at<Vec3b>(i)), -1, 8);
                }
            }

            calcBackProject(std::vector<UMat>(1, hue), vector<int>(1, 0), hist, backproj,
                                vector<float>(hranges, hranges + 2), 1.0);
            bitwise_and(backproj, mask, backproj);

            RotatedRect trackBox = CamShift(backproj, trackWindow,
                              TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));

            if (trackWindow.area() <= 1)
            {
                int cols = backproj.cols, rows = backproj.rows, r = (min(cols, rows) + 5)/6;
                trackWindow = Rect(trackWindow.x - r, trackWindow.y - r, trackWindow.x + r, trackWindow.y + r) & Rect(0, 0, cols, rows);
            }

            if (backprojMode)
                cvtColor(backproj, image, COLOR_GRAY2BGR);

            Mat _image = image.getMat(ACCESS_RW);

            Point2f vertices[4];
            trackBox.points(vertices);
            for (int i = 0; i < 4; i++)
                line(_image, vertices[i], vertices[(i+1)%4], Scalar(0,255,0));

            Rect brect = trackBox.boundingRect();
            rectangle(_image, brect, Scalar(0, 0, 255), 3, LINE_AA);
            ellipse(_image, trackBox, Scalar(0, 255, 0), 3, LINE_AA);
        }
    }
    else if (trackObject < 0)
        paused = false;

    if (selectObject && selection.width > 0 && selection.height > 0)
    {
        UMat roi(image, selection);
        bitwise_not(roi, roi);
    }

    imshow("CamShift Demo", image);
    if (showHist)
        imshow("Histogram", histimg);

    char c = (char)waitKey(10);
    /*if (c == 27)
        break;
    Keyfunction(c);*/
    }
}

//主程式camshift 只輸入一張圖片 選取統計圖固定 用意在抓國旗區塊
void camshift2(Mat img)
{
    help();

    namedWindow("Histogram", WINDOW_NORMAL);
    namedWindow("CamShift Demo", WINDOW_NORMAL);
    setMouseCallback("CamShift Demo", onMouse);
    createTrackbar("Vmin", "CamShift Demo", &vmin, 256);
    createTrackbar("Vmax", "CamShift Demo", &vmax, 256);
    createTrackbar("Smin", "CamShift Demo", &smin, 256);

    img.copyTo(image);

    cvtColor(image, hsv, COLOR_BGR2HSV);

    int _vmin = vmin, _vmax = vmax;

    inRange(hsv, Scalar(0, smin, min(_vmin, _vmax)),
        Scalar(180, 256, max(_vmin, _vmax)), mask);

    int fromTo[2] = { 0,0 };
    hue.create(hsv.size(), hsv.depth());
    mixChannels(vector<UMat>(1, hsv), vector<UMat>(1, hue), fromTo, 1);

    cout<<"trackObject = "<<trackObject<<endl;

    cout << "image.cols = "<<image.cols<<endl;
    cout << "image.rows = "<<image.rows<<endl;

    selection.x = 50;
    selection.y = 50;
    selection.width = 100;
    selection.height = 100;
    selection &= Rect(0, 0, image.cols, image.rows);

    //抓取框選區域統計圖等資料
    UMat roi(hue, selection), maskroi(mask, selection);
    calcHist(vector<Mat>(1, roi.getMat(ACCESS_READ)), vector<int>(1, 0),
                 maskroi, hist, vector<int>(1, hsize), vector<float>(hranges, hranges + 2));
    normalize(hist, hist, 0, 255, NORM_MINMAX);

    //_temp用在存取統計圖
    Mat _temp = hist.getMat(ACCESS_RW);
    for(int i = 0; i < hsize; i++)
        cout << "_temp.at<float>(" << i <<") =" << _temp.at<float>(i) << endl;

    //將統計圖設定成國旗的顏色
    for(int i = 0; i < hsize; i++)
        _temp.at<float>(i) = 0;
    _temp.at<float>(9) = 255;
    _temp.at<float>(15) = 255;

    for(int i = 0; i < hsize; i++)
        cout << "after _temp.at<float>(" << i <<") =" << _temp.at<float>(i) << endl;

    //給予指定區塊 設定flag
    trackWindow = selection;
    trackObject = 1;

    //畫統計圖
    histimg = Scalar::all(0);
    int binW = histimg.cols / hsize;//一條顏色的寬度
    Mat buf (1, hsize, CV_8UC3);
    for (int i = 0; i < hsize; i++)
        buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180./hsize), 255, 255);
    cvtColor(buf, buf, COLOR_HSV2BGR);

    Mat _hist = hist.getMat(ACCESS_READ);
    for (int i = 0; i < hsize; i++)
    {
        int val = saturate_cast<int>(_hist.at<float>(i)*histimg.rows/255);
        rectangle(histimg,
                   Point(i*binW, histimg.rows),
                   Point((i+1)*binW, histimg.rows - val),  // -val 是為了讓數值調往上長
                   Scalar(buf.at<Vec3b>(i)), -1, 8);
    }

    //計算指定顏色在hue圖上的backproject
    calcBackProject(std::vector<UMat>(1, hue), vector<int>(1, 0), hist, backproj,
                        vector<float>(hranges, hranges + 2), 1.0);
    bitwise_and(backproj, mask, backproj);

    //camshift並將結果給予trackBox
    RotatedRect trackBox = CamShift(backproj, trackWindow,
                      TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));

    //排除例外狀況
    if (trackWindow.area() <= 1)
    {
        int cols = backproj.cols, rows = backproj.rows, r = (min(cols, rows) + 5)/6;
        trackWindow = Rect(trackWindow.x - r, trackWindow.y - r, trackWindow.x + r, trackWindow.y + r) & Rect(0, 0, cols, rows);
    }

    //backproject模式
    if (backprojMode)
        cvtColor(backproj, image, COLOR_GRAY2BGR);

    Mat _image = image.getMat(ACCESS_RW);

    //將選擇區域框上綠線
    Point2f vertices[4];
    trackBox.points(vertices);
    for (int i = 0; i < 4; i++)
        line(_image, vertices[i], vertices[(i+1)%4], Scalar(0,255,0));

    //將結果劃上圓形矩形等形狀標注在圖片上
    Rect brect = trackBox.boundingRect();
    rectangle(_image, brect, Scalar(0, 0, 255), 3, LINE_AA);
    ellipse(_image, trackBox, Scalar(0, 255, 0), 3, LINE_AA);

    //取得興趣區域
    if (selectObject && selection.width > 0 && selection.height > 0)
    {
        UMat roi(image, selection);
        bitwise_not(roi, roi);
    }

    //顯示結果
    imshow("CamShift Demo", image);
    if (showHist)
        imshow("Histogram", histimg);
}
