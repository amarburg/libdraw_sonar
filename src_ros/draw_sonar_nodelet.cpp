#include "ros/ros.h"
#include "nodelet/nodelet.h"

#include "acoustic_msgs/SonarImage.h"
#include "imaging_sonar_msgs/ImagingSonarMsg.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

// For uploading drawn sonar images to Image topic
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

#include "sonar_image_proc/ColorMaps.h"
#include "sonar_image_proc/DrawSonar.h"

#include "sonar_image_proc/AbstractSonarInterface.h"

// Subscribes to sonar message topic, draws using opencv then publishes result

namespace draw_sonar {

  // \todo These two can be clean up with templates...
  struct ImagingSonarMsgInterface : public sonar_image_proc::AbstractSonarInterface {

    ImagingSonarMsgInterface( const imaging_sonar_msgs::ImagingSonarMsg::ConstPtr &ping )
      : _ping(ping) {;}

    virtual int nBearings() const         { return _ping->bearings.size(); }

    // Remember that the _original_ ImagingSonarMsg stored bearings as degrees
    virtual float bearing( int n ) const  { return _ping->bearings[n] * M_PI/180; }

    virtual int nRanges() const           { return _ping->ranges.size(); }
    virtual float range( int n ) const    { return _ping->ranges[n]; }

    virtual uint8_t intensity( int i ) const { return _ping->v2intensities[i]; }

    imaging_sonar_msgs::ImagingSonarMsg::ConstPtr _ping;
  };


  struct SonarImageMsgInterface : public sonar_image_proc::AbstractSonarInterface {

    SonarImageMsgInterface( const acoustic_msgs::SonarImage::ConstPtr &ping )
      : _ping(ping) {;}

    int nBearings() const override { return _ping->azimuth_angles.size(); }
    float bearing( int n ) const override { return _ping->azimuth_angles[n]; }

    int nRanges() const override { return _ping->ranges.size(); }
    float range( int n ) const override { return _ping->ranges[n]; }

    uint8_t intensity( int i ) const override {
      if( _ping->data_size == 1 ) {
        return _ping->intensities[i];
      } else if( _ping->data_size == 2 ) {
        uint16_t d;

        if( _ping->is_bigendian)
          d = (_ping->intensities[i * 2] << 8) | _ping->intensities[i * 2 + 1];
        else
          d =  (_ping->intensities[i * 2 + 1] << 8) | _ping->intensities[i * 2];

          // Hacky
          const int shift = 6;
          if( d >= (0x1 << (shift+8)) ) return 0xFF;

        return (d >> shift);
      } else {
        ROS_ERROR_STREAM("SonarImage has unsupported data_size = " << _ping->data_size);
        return 0;
      }

    }

    acoustic_msgs::SonarImage::ConstPtr _ping;
  };


  class DrawSonarNodelet : public nodelet::Nodelet {
  public:

    // NB Color Maps are in the draw_sonar package

    DrawSonarNodelet()
      : Nodelet(),
        _height(0), _width(0), _pixPerRangeBin(2), _maxRange(0.0),
        _colorMap( new sonar_image_proc::InfernoColorMap )
    {;}

    virtual ~DrawSonarNodelet()
    {;}

  private:

    virtual void onInit() {
      ros::NodeHandle nh = getMTNodeHandle();
      ros::NodeHandle pnh = getMTPrivateNodeHandle();

      pnh.param<int>("width", _width, 0);
      pnh.param<int>("height", _height, 0);
      pnh.param<int>("pix_per_range_bin", _pixPerRangeBin, 2 );
      pnh.param<float>("max_range", _maxRange, 0.0 );

      if( _maxRange > 0.0 ) {
        NODELET_INFO_STREAM("Only drawing to max range " << _maxRange );
      }

      subSonarImage_ = nh.subscribe<acoustic_msgs::SonarImage>("sonar_image", 10, &DrawSonarNodelet::sonarImageCallback, this );
      subImagingSonarMsg_ = nh.subscribe<imaging_sonar_msgs::ImagingSonarMsg>("imaging_sonar", 10, &DrawSonarNodelet::imagingSonarCallback, this );

      pub_ = nh.advertise<sensor_msgs::Image>("drawn_sonar", 10);
    }

    void callbackCommon(const cv::Mat &mat, const std_msgs::Header &header) {
      cv_bridge::CvImage img_bridge(header,
                                    sensor_msgs::image_encodings::RGB8,
                                    mat);

      sensor_msgs::Image output_msg;
      img_bridge.toImageMsg(output_msg);
      pub_.publish(output_msg);
    }

    void sonarImageCallback(const acoustic_msgs::SonarImage::ConstPtr &msg) {

      SonarImageMsgInterface interface( msg );

      cv::Size sz = sonar_image_proc::calculateImageSize( interface, cv::Size( _width, _height),
                                                    _pixPerRangeBin, _maxRange );
      cv::Mat mat( sz, CV_8UC3 );
      sonar_image_proc::drawSonar( interface, mat, *_colorMap, _maxRange );

      callbackCommon(mat, msg->header);
    }

    void imagingSonarCallback(const imaging_sonar_msgs::ImagingSonarMsg::ConstPtr &msg) {

      ImagingSonarMsgInterface interface( msg );

      cv::Size sz = sonar_image_proc::calculateImageSize( interface, cv::Size( _width, _height),
                                                    _pixPerRangeBin, _maxRange );
      cv::Mat mat( sz, CV_8UC3 );
      sonar_image_proc::drawSonar( interface, mat, *_colorMap, _maxRange );

      callbackCommon(mat, msg->header);
    }


    ros::Subscriber subSonarImage_, subImagingSonarMsg_;
    ros::Publisher pub_;

    int _height, _width, _pixPerRangeBin;
    float _maxRange;

    std::unique_ptr< sonar_image_proc::SonarColorMap > _colorMap;

  };

}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(draw_sonar::DrawSonarNodelet, nodelet::Nodelet);
