// Detect_track.cpp


#include <iostream>
#include <fstream>
#include <sstream>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

using namespace cv::dnn;
using namespace std;
using namespace cv;

// Initialize the parameters
float objectnessThreshold{ 0.5 };// Objectness threshold
float confThreshold{ 0.5 };		// Confidence threshold
float nmsThreshold{ 0.4 };		// Non-maximum suppression threshold
int inpWidth{ 416 };			// Width of network's input image
int inpHeight{ 416 };			// Height of network's input image
vector<string> classes;
Rect bbox;
Rect2d bbox_track;

// Get the names of the output layers
vector<String> getOutputsNames(const dnn::Net& net);

// Remove the bounding boxes with low confidence using non-maxima suppression
void postprocess(Mat& frame, const vector<Mat>& outs);

// Draw the predicted bounding box
void drawPred(int classId, float conf, int left, int top, int right, int bottom, Mat& frame);


int main(int argc, char** argv)
{
	// Put comandline Parser
	// Load names of classes
	string classesFile{ "coco.names" };
	ifstream ifs(classesFile.c_str());
	
	// Get vector of strings with the names
	string line;
	while (getline(ifs, line))
	{
			classes.push_back(line);
	}



	// Give the configuration and weight files for the model
	String modelConfiguration{ "yolov3.cfg" };
	String modelWeights{ "yolov3.weights" };

	// Load the network
	Net net = readNetFromDarknet(modelConfiguration, modelWeights);
	
	// Read video
	VideoCapture cap("soccer-ball.mp4");

	// Check if video opened successfully
	if (!cap.isOpened()) {
		cout << "Error opening video stream or file" << endl;
		exit(1);
	}

	// CSRT tracker setup
	Ptr<Tracker> tracker;
	tracker = TrackerCSRT::create();

	int count{};

	while (true)
	{
		// Take first frame
		Mat frame;
		cap >> frame;

		if (frame.empty())
		{
			cout << "Frame is empty" << endl;
			break;
		}

		// Detecting part
		if (count == 0)
		{
			// Create a 4D blob from a frame.
			Mat blob;
			double scaleFactor{ 1 / 255.0 };
			blobFromImage(frame, blob, scaleFactor, Size(inpWidth, inpHeight),
				Scalar(0, 0, 0), true, false);

			//Sets the input to the network
			net.setInput(blob);


			// Runs the forward pass to get output of the output layers
			vector<Mat> outs;
			net.forward(outs, getOutputsNames(net));

			// Remove the bounding boxes with low confidence
			postprocess(frame, outs);

			// Frorm Rect to Rect2d object
			Rect2d bbox_track = static_cast<Rect2d>(bbox);

			// Initialize tracker
			bool ok = tracker->init(frame, bbox_track);
			cout << "Tracker init is = " << ok << endl;

			++count;
		}
		// Tracking part
		else if (count <= 9)
		{
			// Update the tracking result
			bool ok = tracker->update(frame, bbox_track);

			// Display bounding box.
			rectangle(frame, bbox_track, Scalar(255, 128, 0), 2, 1);

			

			++count;
		}
		else if (count == 10)
		{
			count = 0;
		}

		imshow("result", frame);
		waitKey(100);


	}
}

// Get the names of the output layers
vector<String> getOutputsNames(const dnn::Net& net)
{
	static vector<String> names;
	if (names.empty())
	{
		//Get the indices of the output layers, i.e. the layers with unconnected outputs
		vector<int> outLayers = net.getUnconnectedOutLayers();

		//get the names of all the layers in the network
		vector<String> layersNames = net.getLayerNames();

		// Get the names of the output layers in names
		names.resize(outLayers.size());
		for (size_t i{}; i < outLayers.size(); ++i)
			names[i] = layersNames[outLayers[i] - 1];
	}
	return names;
}


// Remove the bounding boxes with low confidence using non-maxima suppression
void postprocess(Mat& frame, const vector<Mat>& outs)
{
	vector<int> classIds;
	vector<float> confidences;
	vector<Rect> boxes;
	Rect roi1;

	for (size_t i = 0; i < outs.size(); ++i)
	{
		// Scan through all the bounding boxes output from the network and keep only the
		// ones with high confidence scores. Assign the box's class label as the class
		// with the highest score for the box.
		float* data = (float*)outs[i].data;
		for (int j = 0; j < outs[i].rows; ++j, data += outs[i].cols)
		{
			Mat scores = outs[i].row(j).colRange(5, outs[i].cols);
			Point classIdPoint;
			double confidence;
			// Get the value and location of the maximum score
			minMaxLoc(scores, 0, &confidence, 0, &classIdPoint);
			if (confidence > confThreshold)
			{
				int centerX = (int)(data[0] * frame.cols);
				int centerY = (int)(data[1] * frame.rows);
				int width = (int)(data[2] * frame.cols);
				int height = (int)(data[3] * frame.rows);
				int left = centerX - width / 2;
				int top = centerY - height / 2;

				classIds.push_back(classIdPoint.x);
				confidences.push_back((float)confidence);
				boxes.push_back(Rect(left, top, width, height));
			}
		}
	}

	// Perform non maximum suppression to eliminate redundant overlapping boxes with
	// lower confidences
	vector<int> indices;
	NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);

	int item_num{ 32 };
	for (size_t i = 0; i < indices.size(); ++i)
	{			
		int idx = indices[i];	
		Rect box = boxes[idx];

		// Draw only the sports ball
		if (classIds[idx] == item_num)
		{
			drawPred(classIds[idx], confidences[idx], box.x, box.y,
				box.x + box.width, box.y + box.height, frame);

			// Create bounding box for tracking
			roi1 = Rect(box.x, box.y, box.width, box.height);
			bbox = roi1;
		}
	}
}


// Draw the predicted bounding box
void drawPred(int classId, float conf, int left, int top, int right, int bottom, Mat& frame)
{
	//Draw a rectangle displaying the bounding box
	rectangle(frame, Point(left, top), Point(right, bottom), Scalar(255, 178, 50), 3);

	//Get the label for the class name and its confidence
	string label = format("%.2f", conf);
	if (!classes.empty())
	{
		CV_Assert(classId < (int)classes.size());
		label = classes[classId] + ":" + label;
	}

	//Display the label at the top of the bounding box
	int baseLine;
	Size labelSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
	top = max(top, labelSize.height);
	rectangle(frame, Point(left, top - round(1.5 * labelSize.height)), Point(left + round(1.5 * labelSize.width), top + baseLine), Scalar(255, 255, 255), FILLED);
	putText(frame, label, Point(left, top), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(0, 0, 0), 1);
}