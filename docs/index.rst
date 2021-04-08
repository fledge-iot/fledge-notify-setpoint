.. Images
.. |setpoint_1| image:: images/setpoint_1.jpg


Set Point Control Notification
==============================

The *fledge-notify-setpoint* notification delivery plugin is a mechanism by which a notification can be used to send set point control writes into south services which support set point control

Once you have created your notification rule and move on to the delivery mechanism

  - Select the setpoint plugin from the list of plugins

  - Click *Next*

    +--------------+
    | |setpoint_1| |
    +--------------+

  - Configure the plugin

    - **Service**: The name of the south service you wish to control

    - **Trigger Value**: The set point control payload to send to the south service. This is a list of name, value pairs to be set within the service. These are set when the notification rule triggers.

    - **Cleared Value**: The set point control payload to send to the south service. This is a list of name, value pairs to be set within the service. There are set when the notification rule clears.

  - Enable the plugin and click *Next*

  - Complete your notification setup

