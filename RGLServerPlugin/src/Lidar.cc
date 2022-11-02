#include "RGLServerPluginInstance.hh"
#include "RGLServerPluginManager.hh"

#define RAYS_IN_ONE_DIR 1000

using namespace rgl;

void RGLServerPluginInstance::CreateLidar(ignition::gazebo::Entity entity) {
    updates_between_raytraces = std::chrono::duration_cast<std::chrono::milliseconds>(time_between_raytraces).count();

    ignmsg << "attached to: " << entity << std::endl;
    gazebo_lidar = entity;
    static int next_free_id = 0;
    lidar_id = next_free_id;
    next_free_id++;

    rgl_configure_logging(RGL_LOG_LEVEL_DEBUG, nullptr, true);
    size_t rays = RAYS_IN_ONE_DIR * RAYS_IN_ONE_DIR;
    std::vector<rgl_mat3x4f> ray_tf;
    ignition::math::Angle X;
    ignition::math::Angle Y;
    double unit_angle = ignition::math::Angle::TwoPi.Radian() / RAYS_IN_ONE_DIR;
    for (int i = 0; i < RAYS_IN_ONE_DIR; ++i, X.SetRadian(unit_angle * i)) {
        for (int j = 0; j < RAYS_IN_ONE_DIR; ++j, Y.SetRadian(unit_angle * j)) {
            ignition::math::Quaterniond quaternion(0, X.Radian(), Y.Radian());
            ignition::math::Matrix4d matrix4D(quaternion);
            rgl_mat3x4f rgl_matrix;
            for (int l = 0; l < 3; ++l) {
                for (int m = 0; m < 4; ++m) {
                    rgl_matrix.value[l][m] = static_cast<float>(matrix4D(l, m));
                }
            }
            ray_tf.push_back(rgl_matrix);
        }
    }
    RGL_CHECK(rgl_lidar_create(&rgl_lidar, ray_tf.data(), rays));
    pointcloud_publisher = node.Advertise<ignition::msgs::PointCloudPacked>(
            "/RGL_point_cloud_" + std::to_string(lidar_id));
}



void RGLServerPluginInstance::UpdateLidarPose(const ignition::gazebo::EntityComponentManager& ecm) {
    if (!lidar_exists) {
        return;
    }
    auto rgl_pose_matrix = RGLServerPluginManager::GetRglMatrix(gazebo_lidar, ecm);
    RGL_CHECK(rgl_lidar_set_pose(rgl_lidar, &rgl_pose_matrix));

    /// Debug printf
//    ignmsg << "lidar: " << gazebo_lidar << " rgl_pose_matrix: " << std::endl;
//    for (int i = 0; i < 3; ++i) {
//        for (int j = 0; j < 4; ++j) {
//            ignmsg << rgl_pose_matrix.value[i][j] << " ";
//        }
//        ignmsg << std::endl;
//    }

}

void RGLServerPluginInstance::RayTrace(ignition::gazebo::EntityComponentManager& ecm,
                                       std::chrono::steady_clock::duration sim_time, bool paused) {
    if (!lidar_exists) {
        return;
    }

    current_update++;

    if (!paused && sim_time < last_raytrace_time + time_between_raytraces) {
        return;
    }
    if (!paused) {
        last_raytrace_time = sim_time;
    }

    if (paused && current_update < last_raytrace_update + updates_between_raytraces) {
        return;
    }

    last_raytrace_update = current_update;

    RGL_CHECK(rgl_lidar_raytrace_async(nullptr, rgl_lidar));

    int hitpoint_count = 0;
    RGL_CHECK(rgl_lidar_get_output_size(rgl_lidar, &hitpoint_count));
    if (hitpoint_count == 0) return;
    std::vector<rgl_vec3f> results(hitpoint_count, rgl_vec3f());
    RGL_CHECK(rgl_lidar_get_output_data(rgl_lidar, RGL_FORMAT_XYZ, results.data()));

    ignmsg << "Lidar id: " << lidar_id << " Got " << hitpoint_count << " hitpoint(s)\n";
//    for (int i = 0; i < hitpoint_count; ++i) {
//        ignmsg << " hit: " << i << " coordinates: " << results[i].value[0] << "," << results[i].value[1] << ","
//               << results[i].value[2] << std::endl;
//    }

    // Create message
    ignition::msgs::PointCloudPacked point_cloud_msg;
    ignition::msgs::InitPointCloudPacked(point_cloud_msg, "some_frame", true,
                                         {{"xyz", ignition::msgs::PointCloudPacked::Field::FLOAT32}});
    point_cloud_msg.mutable_data()->resize(hitpoint_count * point_cloud_msg.point_step());
    point_cloud_msg.set_height(1);
    point_cloud_msg.set_width(hitpoint_count);

    // Populate message
    ignition::msgs::PointCloudPackedIterator<float> xIter(point_cloud_msg, "x");
    ignition::msgs::PointCloudPackedIterator<float> yIter(point_cloud_msg, "y");
    ignition::msgs::PointCloudPackedIterator<float> zIter(point_cloud_msg, "z");

    for (int i = 0; i < hitpoint_count; ++i, ++xIter, ++yIter, ++zIter)
    {
        *xIter = results[i].value[0];
        *yIter = results[i].value[1];
        *zIter = results[i].value[2];
    }

    pointcloud_publisher.Publish(point_cloud_msg);
}

bool RGLServerPluginInstance::CheckLidarExists(ignition::gazebo::Entity entity) {
    if (entity == gazebo_lidar) {
        lidar_exists = false;
        rgl_lidar_destroy(rgl_lidar);
        pointcloud_publisher.Publish(ignition::msgs::PointCloudPacked());
    }
    return true;
}
