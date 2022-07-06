#ifndef WEIGHTING_SCALE_SRC_WEIGHTING_H_
#define WEIGHTING_SCALE_SRC_WEIGHTING_H_

#ifdef __cplusplus
extern "C" {
#endif

int weighting_init(void);
int weighting_deinit(void);
int weighting_tare(void);
int weighting_get(double *weight);

#ifdef __cplusplus
}
#endif

#endif // WEIGHTING_SCALE_SRC_WEIGHTING_H_
