#include <iostream>
#include <opencv2/opencv.hpp>
//#include <opencv2/tracking.hpp>
#include "mtcnn.h"
#include <string.h>
#include "tracker.hpp" // use optimised tracker instead of OpenCV version of KCF tracker
#include "utils.h"

#define QUIT_KEY 'q'

using namespace std;
using namespace cv;

VideoCapture getCaptureFromIndexOrIp(const char *str) {
	 if (strcmp(str, "0") == 0 || strcmp(str, "1") == 0) {
        // use camera index
        int camera_id = atoi(str);
        cout << "camera index: " << camera_id << endl;
        VideoCapture camera(camera_id);
		return camera;
    } else {
        string camera_ip = str;
        cout << "camera ip: " << camera_ip << endl;
        string camera_stream = "rtsp://admin:Mcdonalds@" + camera_ip + ":554//Streaming/Channels/1";
        VideoCapture camera(camera_stream);
		return camera;
    }
}

/*
 * Decide whether the detected face is same as the tracking one
 * 
 * return true when:
 *   center point of one box is inside the other
 */
bool isSameFace(Rect2d &box1, Rect2d &box2) {
	int x1 = box1.x + box1.width/2;
	int y1 = box1.y + box1.height/2;
	int x2 = box2.x + box2.width/2;
	int y2 = box2.y + box2.height/2;

	if ( x1 > box2.x && x1 < box2.x + box2.width &&
		 y1 > box2.y && y1 < box2.y + box2.height &&
		 x2 > box1.x && x2 < box1.x + box1.width &&
		 y2 > box1.y && y2 < box1.y + box1.height ) {
			return true;
	}

	return false;
}

void test_video(int argc, char* argv[]) {
	if(argc != 3) {
		cout << "usage: main $model_path $camera_ip" << endl;
		exit(1); 
	}

	string model_path = argv[1];
	MTCNN mm(model_path);

	VideoCapture camera = getCaptureFromIndexOrIp(argv[2]);
    if (!camera.isOpened()) {
        cerr << "failed to open camera" << endl;
        return;
    }

    int counter = 0;
    struct timeval  tv1,tv2;
    struct timezone tz1,tz2;

	vector<Bbox> finalBbox;
    MultiTracker trackers;
	Rect2d roi;
	Mat frame;

	namedWindow("face_detection", WINDOW_NORMAL);

   	do {
		finalBbox.clear();
        camera >> frame;
        if (!frame.data) {
            cerr << "Capture video failed" << endl;
            continue;
        }

		if (counter % 25 == 0) {
			// renew trackers
			//trackers.clear();

			gettimeofday(&tv1,&tz1);
            ncnn::Mat ncnn_img = ncnn::Mat::from_pixels(frame.data, ncnn::Mat::PIXEL_BGR2RGB, frame.cols, frame.rows);
            mm.detect(ncnn_img, finalBbox);
            gettimeofday(&tv2,&tz2);
            int total = 0;
            for(vector<Bbox>::iterator it=finalBbox.begin(); it!=finalBbox.end();it++) {
                if((*it).exist) {
                    total++;
					// draw rectangle
                    //cv::rectangle(frame, cv::Point((*it).x1, (*it).y1), cv::Point((*it).x2, (*it).y2), cv::Scalar(0,0,255), 2,8,0);
                    //for(int num=0;num<5;num++) {
						// draw 5 landmarks
                        //circle(frame, cv::Point((int)*(it->ppoint+num), (int)*(it->ppoint+num+5)), 3, cv::Scalar(0,255,255), -1);
                    //}
					
					// get face bounding box
					auto box = *it;
					Rect2d detectedFace(Point(box.x1, box.y1),Point(box.x2, box.y2));

					// test whether is a new face
					bool newFace = true;
					for (unsigned i=0;i<trackers.getObjects().size();i++) {
						Rect2d trackerFace = trackers.getObjects()[i];
						if (isSameFace(detectedFace, trackerFace)) {
							newFace = false;
							break;
						}
					}
	
					// create a tracker if a new face is detected
					if (newFace) {
						Ptr<Tracker> tracker = TrackerKCF::create();
						trackers.add(tracker, frame, detectedFace);
					}
                }
            }

            cout << "detected " << total << " Persons. time eclipsed: " <<  getElapse(&tv1, &tv2) << " ms" << endl;
		}

		// update trackers
		trackers.update(frame);

		// draw tracked faces
		for(unsigned i=0;i<trackers.getObjects().size();i++)
        	rectangle( frame, trackers.getObjects()[i], Scalar( 255, 0, 0 ), 2, 1 );

		imshow("face_detection", frame);

		counter++;

    } while (QUIT_KEY != waitKey(1));
}

int main(int argc, char* argv[]) {
    test_video(argc, argv);
}
