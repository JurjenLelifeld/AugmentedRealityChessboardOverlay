// Created by Jurjen Lelifeld on 02/21/2017.

#include <opencv2/opencv.hpp>
#include <windows.h>
#include <dshow.h>

#pragma comment(lib, "strmiids")

using namespace std;

void GetConnectedCameras();
HRESULT EnumerateDevices(REFGUID category, IEnumMoniker **ppEnum);
void DisplayDeviceInformation(IEnumMoniker *pEnum);
cv::Point2f getCornerFromCoordinate(int x, int y);

// The chessboard should have black squares in it's corners and an odd number of columns and rows.
// The board width and height should be the number of black and white squares minus one.
// This is to get only the internal corners.
const int BOARD_WIDTH = 14;
const int BOARD_HEIGHT = 6;

// Interior corner points for chessboard calibration image
vector<cv::Point2f> corners;
vector<cv::Point2f> augmentedImageCorners;
vector<cv::Point2f> destinationCorners;

// Instantiate the three output images
cv::Mat original;
cv::Mat chessboard_image;
cv::Mat frameToProcess;

int main()
{
	cout << "Please press 'R' to reset the program after a result has been found." << endl;
	cout << "Please press 'ESC' to stop the program." << endl << endl;		

	// Get the connected camera's and show to user
	GetConnectedCameras();

	// Set up the video capture with desired camera. Ask user input.
	string result = "";
	cout << "Please enter camera number to use: ";
	getline(cin, result);
	cout << endl;

	// Parse user input to int
	int cameranumber = atoi(result.c_str());

	// Start video capture with the webcam
	cv::VideoCapture capture = cv::VideoCapture(cameranumber);

	// Check if capture stream is opened succesfully.
	if (!capture.isOpened())
	{
		cout << "Unable to find video input device." << endl;
		return -1;
	}

	// Load the image we want to overlay on the chessboard
	cv::Mat augmentedImage = cv::imread("golden_gate.jpg");

	// Check if the image has been found
	if (augmentedImage.empty())
	{
		cout << "Unable to read image, check path.\n" << endl;
		return -1;
	}

	// Get the size of our board
	cv::Size boardSize = cv::Size(BOARD_WIDTH, BOARD_HEIGHT);

	// Input image size
	int augmentedImageWidth = augmentedImage.cols;
	int augmentedImageHeight = augmentedImage.rows;

	// Set of source points to calculate the transformation matrix
	augmentedImageCorners.clear();
	augmentedImageCorners.push_back(cv::Point2f(0, 0));
	augmentedImageCorners.push_back(cv::Point2f(augmentedImageWidth, 0));
	augmentedImageCorners.push_back(cv::Point2f(0, augmentedImageHeight));
	augmentedImageCorners.push_back(cv::Point2f(augmentedImageWidth, augmentedImageHeight));

	cout << "Ready to start scanning..." << endl << endl;

	// Keep retrieving new webcam frames until the user stops.
	while (true)
	{
		// Get the latest webcam capture.
		capture >> frameToProcess;

		// Check for invalid input.
		if (!frameToProcess.data)
		{
			cout << "Could not open or find the image" << endl;
			return -1;
		}	

		// Keep the newest frame to show later
		original = frameToProcess.clone();
		chessboard_image = frameToProcess.clone();

		// Try to detect the chessboard
		corners.clear();
		bool framefound = cv::findChessboardCorners(frameToProcess, boardSize, corners,
			CV_CALIB_CB_ADAPTIVE_THRESH |
			CV_CALIB_CB_NORMALIZE_IMAGE |
			CV_CALIB_CB_FAST_CHECK);

		// Draw whatever we found on the chessboard to show the user what has been found
		{
			cv::drawChessboardCorners(chessboard_image, boardSize, corners, framefound);
		}

		if (framefound)
		{
			// Get better accuracy of the corners we found that we can use to calculate the warp matrix
			cv::Mat greyscale;
			cv::cvtColor(frameToProcess, greyscale, cv::COLOR_BGR2GRAY);
			cv::cornerSubPix(greyscale, corners, cv::Size(11, 11), cv::Size(-1, -1), cv::TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));

			// We want to pull out the exterior corners and populate augmentedImageCorners so we can create a
			// transformation matrix.
			destinationCorners.clear();
			destinationCorners.push_back(getCornerFromCoordinate(0, 0)); // Top Left
			destinationCorners.push_back(getCornerFromCoordinate(BOARD_WIDTH - 1, 0)); // Top Right
			destinationCorners.push_back(getCornerFromCoordinate(0, BOARD_HEIGHT - 1)); // Bottom Left
			destinationCorners.push_back(getCornerFromCoordinate(BOARD_WIDTH - 1, BOARD_HEIGHT - 1)); // Bottom Right

			// Create a blank image that is used as canvas
			cv::Mat whiteBlank;
			whiteBlank.create(augmentedImage.rows, augmentedImage.cols, augmentedImage.type());
			whiteBlank = cv::Scalar(0); //empty black image			
			bitwise_not(whiteBlank, whiteBlank); // empty white image			

			// Create empty black images
			cv::Mat transformedInputImage(frameToProcess.rows, frameToProcess.cols, frameToProcess.type());
			transformedInputImage = cv::Scalar(0);
			cv::Mat blackChessboardOverlay(frameToProcess.rows, frameToProcess.cols, frameToProcess.type());
			blackChessboardOverlay = cv::Scalar(0);

			// Compute the transformation matrix
			cv::Mat warp_matrix = cv::getPerspectiveTransform(augmentedImageCorners, destinationCorners);

			// Now transform the image to make it exactly the same way as the chessboard
			cv::warpPerspective(augmentedImage, transformedInputImage, warp_matrix, cv::Size(transformedInputImage.cols, transformedInputImage.rows));

			// Now do the same but create a black plane exactly the same way as the chessboard
			cv::warpPerspective(whiteBlank, blackChessboardOverlay, warp_matrix, cv::Size(blackChessboardOverlay.cols, blackChessboardOverlay.rows));
			bitwise_not(blackChessboardOverlay, blackChessboardOverlay); // Immediately reverse to get the black plane in place of the chessboard

			// Run bitwise operations on the images to replace only the chessboard with the new image inside the captured frame
			// Replace all white pixels with the original frame, leave the black. Bitwise_and only replaces non-black values		
			bitwise_and(blackChessboardOverlay, frameToProcess, blackChessboardOverlay);
			// Now fit the transformed image in. Bitwise_or chooses the non-black pixel value (if available).
			bitwise_or(blackChessboardOverlay, transformedInputImage, frameToProcess); 
		}

		// Now look for real
		corners.clear();

		// Show the newest frames and results to the user.
		cv::imshow("Original", original);
		cv::imshow("Chessboard pattern detected", chessboard_image);
		cv::imshow("Processed", frameToProcess);

		// Give the user the option to exit the program.
		char c = cvWaitKey(30);
		if (c == 27) break;
	}

	// Cleanup
	cv::destroyAllWindows();

	return 0;
}

// Return a specific point from the corners array based on the x and y coordinates
cv::Point2f getCornerFromCoordinate(int x, int y) {
	return corners[(BOARD_WIDTH * y) + x];
}

// Get the connected camera's and show to user
void GetConnectedCameras()
{
	// Retrieve the connected camera's on the system (Windows only!).
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (SUCCEEDED(hr))
	{
		IEnumMoniker *pEnum;

		hr = EnumerateDevices(CLSID_VideoInputDeviceCategory, &pEnum);
		if (SUCCEEDED(hr))
		{
			DisplayDeviceInformation(pEnum);
			pEnum->Release();
		}
		CoUninitialize();
	}
}

// Enumerate the webcams (Windows only).
HRESULT EnumerateDevices(REFGUID category, IEnumMoniker **ppEnum)
{
	// Create the System Device Enumerator.
	ICreateDevEnum *pDevEnum;
	HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
		CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));

	if (SUCCEEDED(hr))
	{
		// Create an enumerator for the category.
		hr = pDevEnum->CreateClassEnumerator(category, ppEnum, 0);
		if (hr == S_FALSE)
		{
			hr = VFW_E_NOT_FOUND;  // The category is empty. Treat as an error.
		}
		pDevEnum->Release();
	}
	return hr;
}

// Display the enumerated devices.
void DisplayDeviceInformation(IEnumMoniker *pEnum)
{
	IMoniker *pMoniker = NULL;
	int i = 0;

	while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
	{
		IPropertyBag *pPropBag;
		HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
		if (FAILED(hr))
		{
			pMoniker->Release();
			continue;
		}

		VARIANT var;
		VariantInit(&var);

		// Get description or friendly name.
		hr = pPropBag->Read(L"Description", &var, 0);
		if (FAILED(hr))
		{
			hr = pPropBag->Read(L"FriendlyName", &var, 0);
		}
		if (SUCCEEDED(hr))
		{
			cout << "Camera " << i << " - ";
			printf("%S \n", var.bstrVal);
			VariantClear(&var);
		}

		hr = pPropBag->Write(L"FriendlyName", &var);

		pPropBag->Release();
		pMoniker->Release();

		i++;
	}
	cout << endl;
}