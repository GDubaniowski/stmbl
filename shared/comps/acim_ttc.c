#include "commands.h"
#include "hal.h"
#include "math.h"
#include "defines.h"
#include "angle.h"

HAL_COMP(acim_ttc);

HAL_PIN(mode);  // 0 = slip, 1 = mtpa, 2 = u/f
HAL_PIN(sensorless);

// motor values
HAL_PIN(torque_n);
HAL_PIN(cur_n);
// HAL_PIN(slip_n);
HAL_PIN(polecount);
HAL_PIN(freq_n);
HAL_PIN(vel_n);
HAL_PIN(u_n);
HAL_PIN(u_boost);
HAL_PIN(t_boost);
HAL_PIN(v_boost);
HAL_PIN(s_boost);

// torque cmd in
HAL_PIN(torque);
HAL_PIN(vel_m);

// cur cmd out
HAL_PIN(d_cmd);
HAL_PIN(q_cmd);
HAL_PIN(cmd_mode);
HAL_PIN(pos);
HAL_PIN(vel_e);
HAL_PIN(slip);

HAL_PIN(t_min);
HAL_PIN(t_max);

static void nrt_init(void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  struct acim_ttc_pin_ctx_t *pins = (struct acim_ttc_pin_ctx_t *)pin_ptr;
  PIN(polecount) = 2;
  PIN(vel_n) = 1745.0 / 60.0 * 2.0 * M_PI;
  PIN(torque_n) = 23.0;
  PIN(cur_n) = 17.0;
  PIN(freq_n) = 60.0;
  PIN(u_n) = 80.0;
  PIN(u_boost) = 7.0;
  PIN(t_boost) = 1.3;
  PIN(v_boost) = 1.15;
  PIN(s_boost) = 2.5;
  PIN(mode) = 0;
  PIN(sensorless) = 0;
}

static void rt_func(float period, void *ctx_ptr, hal_pin_inst_t *pin_ptr) {
  // struct acim_ttc_ctx_t * ctx = (struct acim_ttc_ctx_t *)ctx_ptr;
  struct acim_ttc_pin_ctx_t *pins = (struct acim_ttc_pin_ctx_t *)pin_ptr;

  float poles    = MAX(PIN(polecount), 1.0);
  float vel_n    = PIN(vel_n) * poles;
  float t_n      = MAX(PIN(torque_n), 0.001);
  float freq_n   = MAX(PIN(freq_n), 1.0);
  float slip_n   = vel_n - freq_n * 2.0 * M_PI;
  float cur_n    = PIN(cur_n);
  float u_n      = PIN(u_n);
  float u_boost  = PIN(u_boost);
  float t_boost  = PIN(t_boost);
  float v_boost  = PIN(v_boost);

  float torque   = PIN(torque);
  float vel      = 0.0;
  if(PIN(sensorless) > 0.0){
    vel = PIN(vel_e);
  }
  else{
    vel = PIN(vel_m) * poles;
  }

  float d_cmd    = 0.0;
  float q_cmd    = 0.0;
  float slip     = 0.0;
  float cmd_mode = 0;

  float id_n = cur_n / sqrtf(2.0);

  switch((int)PIN(mode)) {
    case 0: // slip control
      cmd_mode = 1.0; // cur cmd
      d_cmd = MIN(id_n, id_n * freq_n * 2.0 * M_PI * v_boost / vel); // constant flux
      q_cmd   = id_n / t_n * torque;
      slip = slip_n / t_n * torque;
      break;

    case 1: // mtpa
      cmd_mode = 1.0; // cur cmd
      d_cmd   = 0.0;
      q_cmd   = cur_n / t_n * torque;
      slip = slip_n * SIGN(torque);  // constant slip
      break;

    case 2: // u/f slip
      cmd_mode = 0; // volt cmd
      slip = slip_n / t_n * torque;
      d_cmd = MAX(u_n / freq_n * ABS(vel / 2.0 / M_PI), u_boost);
      q_cmd = 0.0;
      break;

    default:
      cmd_mode = 1.0; // cur cmd
      d_cmd   = 0;
      q_cmd   = 0;
      slip = 0.0;
  }

  float t_min = 0;
  float t_max = 0;

  if(PIN(vel_m) > 0.0){
    t_max = t_n * MIN(t_boost, vel_n * v_boost / vel);
    t_min = -t_max * t_boost;
  }
  else{
    t_min = -t_n * MIN(t_boost, vel_n * v_boost / vel);
    t_max = -t_min * t_boost;
  }

  slip = LIMIT(slip, slip_n * PIN(s_boost));

  if(PIN(sensorless) > 0.0){
    vel -= slip;
    PIN(vel_m) = vel * poles;
  }
  else{
    vel += slip;
    PIN(vel_e) = vel;
  }

  PIN(t_min) = t_min;
  PIN(t_max) = t_max;

  PIN(cmd_mode) = cmd_mode;
  PIN(d_cmd)   = d_cmd;
  PIN(q_cmd)   = q_cmd;

  PIN(slip) = slip;
  PIN(pos)  = mod(PIN(pos) + vel * period);
}

hal_comp_t acim_ttc_comp_struct = {
    .name      = "acim_ttc",
    .nrt       = 0,
    .rt        = rt_func,
    .frt       = 0,
    .nrt_init  = nrt_init,
    .rt_start  = 0,
    .frt_start = 0,
    .rt_stop   = 0,
    .frt_stop  = 0,
    .ctx_size  = 0,
    .pin_count = sizeof(struct acim_ttc_pin_ctx_t) / sizeof(struct hal_pin_inst_t),
};
