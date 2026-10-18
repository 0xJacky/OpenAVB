#pragma once

#include "avp/image/boot/boot_image.h"


/* Publishes the metadata committed by the image evaluator after
 * every validation and materialization step has succeeded. */
int platform_boot_policy_publish_image_evaluation(
    const AVPBootImageView *image,
    const AVPBootImagePolicy *policy
);
