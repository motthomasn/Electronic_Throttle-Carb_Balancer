# Electronic Throttle/Carburettor Balancer

Electronic interface for balancing individual throttle bodies or carburettors using port pressure sensors. Currently the interface assumes the port pressure sensors are already fitted to the vehicle. i.e. my QuadMAP-CMP_Sensor assembly: https://github.com/motthomasn/QuadMAP-CMP_Sensor. A similar external board would be required if it does not exist.

The interface is designed to be as flexible as possible, allowing for between 2-6 cylinders.

Balancing is done by looking at the minimum port pressure on each sensor across a cycle. Results can be averaged across the previous *n* cycles for smoother reporting. Live statistical data is presented on a TFT display in graphical form.