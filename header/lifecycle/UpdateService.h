#ifndef WIN_SWITCHER_UPDATESERVICE_H
#define WIN_SWITCHER_UPDATESERVICE_H

class UpdateService {
public:
    UpdateService() = default;

    bool handleUpdateRollback();
    void cleanupUpdateMarkers();

private:
    static bool tryRollback();
};

#endif //WIN_SWITCHER_UPDATESERVICE_H
