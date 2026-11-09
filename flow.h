/* flow.h -- header file for flow control module.
 * Author: Luis Colorado <luiscoloradourcola@gmail.com>
 * Date: Mon Nov  9 08:22:18 EET 2026
 * Copyright: (c) 2026 Luis Colorado.  All rights reserved.
 * License: BSD
 */
#ifndef FLOW_H_03d9c294_dc06_11f1_a067_0023ae68f329
#define FLOW_H_03d9c294_dc06_11f1_a067_0023ae68f329

#define LOW_WATER_MARK                               (10)
#define HIGH_WATER_MARK (RB_BUFFER_SIZE - LOW_WATER_MARK)

typedef void (flow_control_t)(struct pthread_info *pi);

flow_control_t
        downstream_flow_control,
        upstream_flow_control;

#endif /* FLOW_H_03d9c294_dc06_11f1_a067_0023ae68f329 */
