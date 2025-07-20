#ifndef SD_ERROR_VIEW_H
#define SD_ERROR_VIEW_H

#include "../view.h"

class SdErrorView : public View {
public:
    void create(lv_obj_t* parent) override;
    ~SdErrorView() override;
};

#endif // SD_ERROR_VIEW_H