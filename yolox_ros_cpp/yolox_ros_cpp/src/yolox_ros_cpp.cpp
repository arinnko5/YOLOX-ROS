#include "yolox_ros_cpp/yolox_ros_cpp.hpp"

namespace yolox_ros_cpp{

    YoloXNode::YoloXNode(const rclcpp::NodeOptions& options)
    : YoloXNode::YoloXNode("", options)
    {}

    YoloXNode::YoloXNode(const std::string &node_name, const rclcpp::NodeOptions& options)
    : rclcpp::Node("yolox_ros_cpp", node_name, options)
    {
        RCLCPP_INFO(this->get_logger(), "initialize");
        this->initializeParameter();

        if(this->imshow_){
            char window_name[50];
            sprintf(window_name, "%s %s %s", this->WINDOW_NAME_.c_str(), "_", this->get_name());
            this->WINDOW_NAME_ = window_name;

            cv::namedWindow(this->WINDOW_NAME_, cv::WINDOW_AUTOSIZE);
        }

        if(this->class_labels_path_!="")
        {
            RCLCPP_INFO(this->get_logger(), "read class labels from '%s'", this->class_labels_path_.c_str());
            this->class_names_ = yolox_cpp::utils::read_class_labels_file(this->class_labels_path_);
        }
        else
        {
            this->class_names_ = yolox_cpp::COCO_CLASSES;
        }

        if(this->model_type_ == "tensorrt"){
            #ifdef ENABLE_TENSORRT
                RCLCPP_INFO(this->get_logger(), "Model Type is TensorRT");
                this->yolox_ = std::make_unique<yolox_cpp::YoloXTensorRT>(this->model_path_, this->tensorrt_device_,
                                                                          this->nms_th_, this->conf_th_, this->model_version_,
                                                                          this->num_classes_);
            #else
                RCLCPP_ERROR(this->get_logger(), "yolox_cpp is not built with TensorRT");
                rclcpp::shutdown();
            #endif
        }else if(this->model_type_ == "openvino"){
            #ifdef ENABLE_OPENVINO
                RCLCPP_INFO(this->get_logger(), "Model Type is OpenVINO");
                this->yolox_ = std::make_unique<yolox_cpp::YoloXOpenVINO>(this->model_path_, this->openvino_device_,
                                                                          this->nms_th_, this->conf_th_, this->model_version_,
                                                                          this->num_classes_);
            #else
                RCLCPP_ERROR(this->get_logger(), "yolox_cpp is not built with OpenVINO");
                rclcpp::shutdown();
            #endif
        }else if(this->model_type_ == "onnxruntime"){
            #ifdef ENABLE_ONNXRUNTIME
                RCLCPP_INFO(this->get_logger(), "Model Type is ONNXRuntime");
                this->yolox_ = std::make_unique<yolox_cpp::YoloXONNXRuntime>(this->model_path_,
                                                                             this->onnxruntime_intra_op_num_threads_,
                                                                             this->onnxruntime_inter_op_num_threads_,
                                                                             this->onnxruntime_use_cuda_, this->onnxruntime_device_id_,
                                                                             this->onnxruntime_use_parallel_,
                                                                             this->nms_th_, this->conf_th_, this->model_version_,
                                                                             this->num_classes_
                                                                            );
            #else
                RCLCPP_ERROR(this->get_logger(), "yolox_cpp is not built with ONNXRuntime");
                rclcpp::shutdown();
            #endif
        }
        RCLCPP_INFO(this->get_logger(), "model loaded");

        this->sub_image_ = image_transport::create_subscription(
            this, this->src_image_topic_name_,
            std::bind(&YoloXNode::colorImageCallback, this, std::placeholders::_1),
            "raw");
        this->pub_bboxes_ = this->create_publisher<bboxes_ex_msgs::msg::BoundingBoxes>(
            this->publish_boundingbox_topic_name_,
            10
        );
        this->pub_image_ = image_transport::create_publisher(this, this->publish_image_topic_name_);

    }

    void YoloXNode::initializeParameter()
    {
        this->declare_parameter<bool>("imshow_isshow", true);
        this->declare_parameter<std::string>("model_path", "src/YOLOX-ROS/weights/openvino/yolox_tiny.xml");
        this->declare_parameter<std::string>("class_labels_path", "");
        this->declare_parameter<int>("num_classes", 80);
        this->declare_parameter<float>("conf", 0.3f);
        this->declare_parameter<float>("nms", 0.45f);
        this->declare_parameter<int>("tensorrt/device", 0);
        this->declare_parameter<std::string>("openvino/device", "CPU");
        this->declare_parameter<bool>("onnxruntime/use_cuda", true);
        this->declare_parameter<int>("onnxruntime/device_id", 0);
        this->declare_parameter<bool>("onnxruntime/use_parallel", false);
        this->declare_parameter<int>("onnxruntime/inter_op_num_threads", 1);
        this->declare_parameter<int>("onnxruntime/intra_op_num_threads", 1);
        this->declare_parameter<std::string>("model_type", "openvino");
        this->declare_parameter<std::string>("model_version", "0.1.1rc0");
        this->declare_parameter<std::string>("src_image_topic_name", "image_raw");
        this->declare_parameter<std::string>("publish_image_topic_name", "yolox/image_raw");
        this->declare_parameter<std::string>("publish_boundingbox_topic_name", "yolox/bounding_boxes");

        this->get_parameter("imshow_isshow", this->imshow_);
        this->get_parameter("model_path", this->model_path_);
        this->get_parameter("class_labels_path", this->class_labels_path_);
        this->get_parameter("num_classes", this->num_classes_);
        this->get_parameter("conf", this->conf_th_);
        this->get_parameter("nms", this->nms_th_);
        this->get_parameter("tensorrt/device", this->tensorrt_device_);
        this->get_parameter("openvino/device", this->openvino_device_);
        this->get_parameter("onnxruntime/use_cuda", this->onnxruntime_use_cuda_);
        this->get_parameter("onnxruntime/device_id", this->onnxruntime_device_id_);
        this->get_parameter("onnxruntime/use_parallel", this->onnxruntime_use_parallel_);
        this->get_parameter("onnxruntime/inter_op_num_threads", this->onnxruntime_inter_op_num_threads_);
        this->get_parameter("onnxruntime/intra_op_num_threads", this->onnxruntime_intra_op_num_threads_);
        this->get_parameter("model_type", this->model_type_);
        this->get_parameter("model_version", this->model_version_);
        this->get_parameter("src_image_topic_name", this->src_image_topic_name_);
        this->get_parameter("publish_image_topic_name", this->publish_image_topic_name_);
        this->get_parameter("publish_boundingbox_topic_name", this->publish_boundingbox_topic_name_);

        RCLCPP_INFO(this->get_logger(), "Set parameter imshow_isshow: %i", this->imshow_);
        RCLCPP_INFO(this->get_logger(), "Set parameter model_path: '%s'", this->model_path_.c_str());
        RCLCPP_INFO(this->get_logger(), "Set parameter class_labels_path: '%s'", this->class_labels_path_.c_str());
        RCLCPP_INFO(this->get_logger(), "Set parameter num_classes: %i", this->num_classes_);
        RCLCPP_INFO(this->get_logger(), "Set parameter conf: %f", this->conf_th_);
        RCLCPP_INFO(this->get_logger(), "Set parameter nms: %f", this->nms_th_);
        RCLCPP_INFO(this->get_logger(), "Set parameter tensorrt/device: %i", this->tensorrt_device_);
        RCLCPP_INFO(this->get_logger(), "Set parameter openvino/device: %s", this->openvino_device_.c_str());
        RCLCPP_INFO(this->get_logger(), "Set parameter onnxruntime/use_cuda: %i", this->onnxruntime_use_cuda_);
        RCLCPP_INFO(this->get_logger(), "Set parameter onnxruntime/device_id: %i", this->onnxruntime_device_id_);
        RCLCPP_INFO(this->get_logger(), "Set parameter onnxruntime/use_parallel: %i", this->onnxruntime_use_parallel_);
        RCLCPP_INFO(this->get_logger(), "Set parameter model_type: '%s'", this->model_type_.c_str());
        RCLCPP_INFO(this->get_logger(), "Set parameter model_version: '%s'", this->model_version_.c_str());
        RCLCPP_INFO(this->get_logger(), "Set parameter src_image_topic_name: '%s'", this->src_image_topic_name_.c_str());
        RCLCPP_INFO(this->get_logger(), "Set parameter publish_image_topic_name: '%s'", this->publish_image_topic_name_.c_str());

    }
    void YoloXNode::colorImageCallback(const sensor_msgs::msg::Image::ConstSharedPtr& ptr)
    {
        auto img = cv_bridge::toCvCopy(ptr, "bgr8");
        cv::Mat frame = img->image;

        // fps
        auto now = std::chrono::system_clock::now();

        auto objects = this->yolox_->inference(frame);

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
        RCLCPP_INFO(this->get_logger(), "Inference: %f FPS", 1000.0f / elapsed.count());

        yolox_cpp::utils::draw_objects(frame, objects, this->class_names_);
        if(this->imshow_){
            cv::imshow(this->WINDOW_NAME_, frame);
            auto key = cv::waitKey(1);
            if(key == 27){
                rclcpp::shutdown();
            }
        }

        auto boxes = objects_to_bboxes(frame, objects, img->header);
        this->pub_bboxes_->publish(boxes);

        sensor_msgs::msg::Image::SharedPtr pub_img;
        pub_img = cv_bridge::CvImage(img->header, "bgr8", frame).toImageMsg();
        this->pub_image_.publish(pub_img);

    }
    bboxes_ex_msgs::msg::BoundingBoxes YoloXNode::objects_to_bboxes(cv::Mat frame, std::vector<yolox_cpp::Object> objects, std_msgs::msg::Header header)
    {
        bboxes_ex_msgs::msg::BoundingBoxes boxes;
        boxes.header = header;
        for(auto obj: objects){
            bboxes_ex_msgs::msg::BoundingBox box;
            box.probability = obj.prob;
            box.class_id = yolox_cpp::COCO_CLASSES[obj.label];
            box.xmin = obj.rect.x;
            box.ymin = obj.rect.y;
            box.xmax = (obj.rect.x + obj.rect.width);
            box.ymax = (obj.rect.y + obj.rect.height);
            box.img_width = frame.cols;
            box.img_height = frame.rows;
            // tracking id
            // box.id = 0;
            // depth
            // box.center_dist = 0;
            boxes.bounding_boxes.emplace_back(box);
        }
        return boxes;
    }
}

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::NodeOptions node_options;
  node_options.enable_topic_statistics(true);
  rclcpp::spin(std::make_shared<yolox_ros_cpp::YoloXNode>(node_options));
  rclcpp::shutdown();
  return 0;
}

RCLCPP_COMPONENTS_REGISTER_NODE(yolox_ros_cpp::YoloXNode)

