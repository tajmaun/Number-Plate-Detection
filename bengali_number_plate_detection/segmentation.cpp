#include <iostream>
#include <vector>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
//using namespace std;
/* The code has an outter loop where every iteration processes one of the four input images */
int main()
{

std::string files[] = { "side.jpg", "side.jpg", "side.jpg", "side.jpg" };
cv::Mat imgs[4];
for (int a = 0; a < 1; a++)
{
    /* Load input image */

    imgs[a] = cv::imread(files[a]);
    if (imgs[a].empty())
    {
        std::cout << "!!! Failed to open image: " << imgs[a] << std::endl;
        return -1;
    }

    /* Convert to grayscale */

    cv::Mat gray;
    cv::cvtColor(imgs[a], gray, cv::COLOR_BGR2GRAY);

    /* Histogram equalization improves the contrast between dark/bright areas */

    cv::Mat equalized;
    cv::equalizeHist(gray, equalized);
    cv::imwrite(std::string("eq_" + std::to_string(a) + ".jpg"), equalized);
    cv::imshow("Hist. Eq.", equalized);

    /* Bilateral filter helps to improve the segmentation process */

    cv::Mat blur;
    cv::bilateralFilter(equalized, blur, 9, 75, 75);
    cv::imwrite(std::string("filter_" + std::to_string(a) + ".jpg"), blur);
    cv::imshow("Filter", blur);

    /* Threshold to binarize the image */

    cv::Mat thres;
    cv::adaptiveThreshold(blur, thres, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 15, 2); //15, 2
    cv::imwrite(std::string("thres_" + std::to_string(a) + ".jpg"), thres);
    cv::imshow("Threshold", thres);

    /* Remove small segments and the extremelly large ones as well */

    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(thres, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    double min_area = 50;
    double max_area = 2000;
    std::vector<std::vector<cv::Point> > good_contours;
    for (size_t i = 0; i < contours.size(); i++)
    {
        double area = cv::contourArea(contours[i]);
        if (area > min_area && area < max_area)
            good_contours.push_back(contours[i]);
    }

    cv::Mat segments(gray.size(), CV_8U, cv::Scalar(255));
    cv::drawContours(segments, good_contours, -1, cv::Scalar(0), -1, 4);
    cv::imwrite(std::string("segments_" + std::to_string(a) + ".jpg"), segments);
    cv::imshow("Segments", segments);

    /* Examine the segments that survived the previous lame filtering process
     * to figure out the top and bottom heights of the largest segments.
     * This info will be used to remove segments that are not aligned with
     * the letters/numbers of the plate.
     * This technique is super flawed for other types of input images.
     */

    // Figure out the average of the top/bottom heights of the largest segments
    int min_average_y = 0, max_average_y = 0, count = 0;
    for (size_t i = 0; i < good_contours.size(); i++)
    {
        std::vector<cv::Point> c = good_contours[i];
        double area = cv::contourArea(c);
        if (area > 200)
        {
            int min_y = segments.rows, max_y = 0;
            for (size_t j = 0; j < c.size(); j++)
            {
                if (c[j].y < min_y)
                    min_y = c[j].y;

                if (c[j].y > max_y)
                    max_y = c[j].y;
            }
            min_average_y += min_y;
            max_average_y += max_y;
            count++;
        }
    }
    min_average_y /= count;
    max_average_y /= count;
    //std::cout << "Average min: " << min_average_y << " max: " << max_average_y << std::endl;

    // Create a new vector of contours with just the ones that fall within the min/max Y
    std::vector<std::vector<cv::Point> > final_contours;
    for (size_t i = 0; i < good_contours.size(); i++)
    {
        std::vector<cv::Point> c = good_contours[i];
        int min_y = segments.rows, max_y = 0;
        for (size_t j = 0; j < c.size(); j++)
        {
            if (c[j].y < min_y)
                min_y = c[j].y;

            if (c[j].y > max_y)
                max_y = c[j].y;
        }

        // 5 is to add a little tolerance from the average Y coordinate
        if (min_y >= (min_average_y-5) && (max_y <= max_average_y+5))
            final_contours.push_back(c);
    }

    cv::Mat final(gray.size(), CV_8U, cv::Scalar(255));
    cv::drawContours(final, final_contours, -1, cv::Scalar(0), -1, 4);
    cv::imwrite(std::string("final_" + std::to_string(a) + ".jpg"), final);
    cv::imshow("Final", final);


    // Create a single vector with all the points that make the segments
    std::vector<cv::Point> points;
    for (size_t x = 0; x < final_contours.size(); x++)
    {
        std::vector<cv::Point> c = final_contours[x];
        for (size_t y = 0; y < c.size(); y++)
            points.push_back(c[y]);
    }

    // Compute a single bounding box for the points
    cv::RotatedRect box = cv::minAreaRect(cv::Mat(points));
    cv::Rect roi;
    roi.x = box.center.x - (box.size.width / 2);
    roi.y = box.center.y - (box.size.height / 2);
    roi.width = box.size.width;
    roi.height = box.size.height;

    // Draw the box at on equalized image
    cv::Point2f vertices[4];
    box.points(vertices);
    for(int i = 0; i < 4; ++i)
        cv::line(imgs[a], vertices[i], vertices[(i + 1) % 4], cv::Scalar(255, 0, 0), 1, CV_AA);
    cv::imwrite(std::string("box_" + std::to_string(a) + ".jpg"), imgs[a]);
    cv::imshow("Box", imgs[a]);

    // Crop the equalized image with the area defined by the ROI
    cv::Mat crop = equalized(roi);
    cv::imwrite(std::string("crop_" + std::to_string(a) + ".jpg"), crop);
    cv::imshow("crop", crop);

    /* The cropped image should contain only the plate's letters and numbers.
     * From here on you can use your own techniques to segment the characters properly.
     */

    cv::waitKey(0);
}
}
