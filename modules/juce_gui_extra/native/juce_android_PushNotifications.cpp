/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (constructor,             "<init>",                  "(Ljava/lang/String;Ljava/lang/CharSequence;I)V") \
  METHOD (enableLights,            "enableLights",            "(Z)V") \
  METHOD (enableVibration,         "enableVibration",         "(Z)V") \
  METHOD (setBypassDnd,            "setBypassDnd",            "(Z)V") \
  METHOD (setDescription,          "setDescription",          "(Ljava/lang/String;)V") \
  METHOD (setGroup,                "setGroup",                "(Ljava/lang/String;)V") \
  METHOD (setImportance,           "setImportance",           "(I)V") \
  METHOD (setLightColor,           "setLightColor",           "(I)V") \
  METHOD (setLockscreenVisibility, "setLockscreenVisibility", "(I)V") \
  METHOD (setShowBadge,            "setShowBadge",            "(Z)V") \
  METHOD (setSound,                "setSound",                "(Landroid/net/Uri;Landroid/media/AudioAttributes;)V") \
  METHOD (setVibrationPattern,     "setVibrationPattern",     "([J)V")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationChannel, "android/app/NotificationChannel", 26)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (constructor, "<init>", "(Ljava/lang/String;Ljava/lang/CharSequence;)V")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationChannelGroup, "android/app/NotificationChannelGroup", 26)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  FIELD (extras, "extras", "Landroid/os/Bundle;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (AndroidNotification, "android/app/Notification", 19)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (addExtras,      "addExtras",      "(Landroid/os/Bundle;)Landroid/app/Notification$Action$Builder;") \
  METHOD (addRemoteInput, "addRemoteInput", "(Landroid/app/RemoteInput;)Landroid/app/Notification$Action$Builder;") \
  METHOD (constructor,    "<init>",         "(ILjava/lang/CharSequence;Landroid/app/PendingIntent;)V") \
  METHOD (build,          "build",          "()Landroid/app/Notification$Action;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationActionBuilder, "android/app/Notification$Action$Builder", 20)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (getNotification,  "getNotification",  "()Landroid/app/Notification;") \
  METHOD (setAutoCancel,    "setAutoCancel",    "(Z)Landroid/app/Notification$Builder;") \
  METHOD (setContentInfo,   "setContentInfo",   "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;") \
  METHOD (setContentIntent, "setContentIntent", "(Landroid/app/PendingIntent;)Landroid/app/Notification$Builder;") \
  METHOD (setContentText,   "setContentText",   "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;") \
  METHOD (setContentTitle,  "setContentTitle",  "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;") \
  METHOD (setDefaults,      "setDefaults",      "(I)Landroid/app/Notification$Builder;") \
  METHOD (setDeleteIntent,  "setDeleteIntent",  "(Landroid/app/PendingIntent;)Landroid/app/Notification$Builder;") \
  METHOD (setLargeIcon,     "setLargeIcon",     "(Landroid/graphics/Bitmap;)Landroid/app/Notification$Builder;") \
  METHOD (setLights,        "setLights",        "(III)Landroid/app/Notification$Builder;") \
  METHOD (setNumber,        "setNumber",        "(I)Landroid/app/Notification$Builder;") \
  METHOD (setOngoing,       "setOngoing",       "(Z)Landroid/app/Notification$Builder;") \
  METHOD (setOnlyAlertOnce, "setOnlyAlertOnce", "(Z)Landroid/app/Notification$Builder;") \
  METHOD (setProgress,      "setProgress",      "(IIZ)Landroid/app/Notification$Builder;") \
  METHOD (setSmallIcon,     "setSmallIcon",     "(I)Landroid/app/Notification$Builder;") \
  METHOD (setSound,         "setSound",         "(Landroid/net/Uri;)Landroid/app/Notification$Builder;") \
  METHOD (setTicker,        "setTicker",        "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;") \
  METHOD (setVibrate,       "setVibrate",       "([J)Landroid/app/Notification$Builder;") \
  METHOD (setWhen,          "setWhen",          "(J)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderBase, "android/app/Notification$Builder", 11)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
   METHOD (addAction,          "addAction",          "(ILjava/lang/CharSequence;Landroid/app/PendingIntent;)Landroid/app/Notification$Builder;") \
   METHOD (build,              "build",              "()Landroid/app/Notification;") \
   METHOD (setPriority,        "setPriority",        "(I)Landroid/app/Notification$Builder;") \
   METHOD (setSubText,         "setSubText",         "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;") \
   METHOD (setUsesChronometer, "setUsesChronometer", "(Z)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderApi16, "android/app/Notification$Builder", 16)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
    METHOD (setShowWhen, "setShowWhen", "(Z)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderApi17, "android/app/Notification$Builder", 17)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (addAction,       "addAction",       "(Landroid/app/Notification$Action;)Landroid/app/Notification$Builder;") \
  METHOD (addExtras,       "addExtras",       "(Landroid/os/Bundle;)Landroid/app/Notification$Builder;") \
  METHOD (setLocalOnly,    "setLocalOnly",    "(Z)Landroid/app/Notification$Builder;") \
  METHOD (setGroup,        "setGroup",        "(Ljava/lang/String;)Landroid/app/Notification$Builder;") \
  METHOD (setGroupSummary, "setGroupSummary", "(Z)Landroid/app/Notification$Builder;") \
  METHOD (setSortKey,      "setSortKey",      "(Ljava/lang/String;)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderApi20, "android/app/Notification$Builder", 20)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (addPerson,        "addPerson",        "(Ljava/lang/String;)Landroid/app/Notification$Builder;") \
  METHOD (setCategory,      "setCategory",      "(Ljava/lang/String;)Landroid/app/Notification$Builder;") \
  METHOD (setColor,         "setColor",         "(I)Landroid/app/Notification$Builder;") \
  METHOD (setPublicVersion, "setPublicVersion", "(Landroid/app/Notification;)Landroid/app/Notification$Builder;") \
  METHOD (setVisibility,    "setVisibility",    "(I)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderApi21, "android/app/Notification$Builder", 21)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (setChronometerCountDown, "setChronometerCountDown", "(Z)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderApi24, "android/app/Notification$Builder", 24)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (setBadgeIconType,      "setBadgeIconType",      "(I)Landroid/app/Notification$Builder;") \
  METHOD (setGroupAlertBehavior, "setGroupAlertBehavior", "(I)Landroid/app/Notification$Builder;") \
  METHOD (setTimeoutAfter,       "setTimeoutAfter",       "(J)Landroid/app/Notification$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationBuilderApi26, "android/app/Notification$Builder", 26)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (cancel,    "cancel",    "(Ljava/lang/String;I)V") \
  METHOD (cancelAll, "cancelAll", "()V") \
  METHOD (notify,    "notify",    "(Ljava/lang/String;ILandroid/app/Notification;)V")

DECLARE_JNI_CLASS (NotificationManagerBase, "android/app/NotificationManager")
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (getActiveNotifications, "getActiveNotifications", "()[Landroid/service/notification/StatusBarNotification;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationManagerApi23, "android/app/NotificationManager", 23)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (areNotificationsEnabled, "areNotificationsEnabled", "()Z")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationManagerApi24, "android/app/NotificationManager", 24)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (createNotificationChannel,      "createNotificationChannel",      "(Landroid/app/NotificationChannel;)V") \
  METHOD (createNotificationChannelGroup, "createNotificationChannelGroup", "(Landroid/app/NotificationChannelGroup;)V")

DECLARE_JNI_CLASS_WITH_MIN_SDK (NotificationManagerApi26, "android/app/NotificationManager", 26)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  STATICMETHOD (getResultsFromIntent, "getResultsFromIntent", "(Landroid/content/Intent;)Landroid/os/Bundle;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (RemoteInput, "android/app/RemoteInput", 20)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (constructor,           "<init>",                "(Ljava/lang/String;)V") \
  METHOD (build,                 "build",                 "()Landroid/app/RemoteInput;") \
  METHOD (setAllowFreeFormInput, "setAllowFreeFormInput", "(Z)Landroid/app/RemoteInput$Builder;") \
  METHOD (setChoices,            "setChoices",            "([Ljava/lang/CharSequence;)Landroid/app/RemoteInput$Builder;") \
  METHOD (setLabel,              "setLabel",              "(Ljava/lang/CharSequence;)Landroid/app/RemoteInput$Builder;")

DECLARE_JNI_CLASS_WITH_MIN_SDK (RemoteInputBuilder, "android/app/RemoteInput$Builder", 20)
#undef JNI_CLASS_MEMBERS

#define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
  METHOD (getNotification, "getNotification", "()Landroid/app/Notification;")

 DECLARE_JNI_CLASS_WITH_MIN_SDK (StatusBarNotification, "android/service/notification/StatusBarNotification", 23)
 #undef JNI_CLASS_MEMBERS

//==============================================================================
#if defined(JUCE_FIREBASE_INSTANCE_ID_SERVICE_CLASSNAME)
 #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
   STATICMETHOD (getInstance, "getInstance", "()Lcom/google/firebase/iid/FirebaseInstanceId;") \
   METHOD (getToken, "getToken", "()Ljava/lang/String;")

 DECLARE_JNI_CLASS (FirebaseInstanceId, "com/google/firebase/iid/FirebaseInstanceId")
 #undef JNI_CLASS_MEMBERS
#endif

#if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
 #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
   STATICMETHOD (getInstance, "getInstance", "()Lcom/google/firebase/messaging/FirebaseMessaging;") \
   METHOD (send,                 "send",                 "(Lcom/google/firebase/messaging/RemoteMessage;)V") \
   METHOD (subscribeToTopic,     "subscribeToTopic",     "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;") \
   METHOD (unsubscribeFromTopic, "unsubscribeFromTopic", "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;") \

 DECLARE_JNI_CLASS (FirebaseMessaging, "com/google/firebase/messaging/FirebaseMessaging")
 #undef JNI_CLASS_MEMBERS

 #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
   METHOD (getCollapseKey,  "getCollapseKey",  "()Ljava/lang/String;") \
   METHOD (getData,         "getData",         "()Ljava/util/Map;") \
   METHOD (getFrom,         "getFrom",         "()Ljava/lang/String;") \
   METHOD (getMessageId,    "getMessageId",    "()Ljava/lang/String;") \
   METHOD (getMessageType,  "getMessageType",  "()Ljava/lang/String;") \
   METHOD (getNotification, "getNotification", "()Lcom/google/firebase/messaging/RemoteMessage$Notification;") \
   METHOD (getSentTime,     "getSentTime",     "()J") \
   METHOD (getTo,           "getTo",           "()Ljava/lang/String;") \
   METHOD (getTtl,          "getTtl",          "()I")

 DECLARE_JNI_CLASS (RemoteMessage, "com/google/firebase/messaging/RemoteMessage")
 #undef JNI_CLASS_MEMBERS

  #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
   METHOD (addData,        "addData",        "(Ljava/lang/String;Ljava/lang/String;)Lcom/google/firebase/messaging/RemoteMessage$Builder;") \
   METHOD (build,          "build",          "()Lcom/google/firebase/messaging/RemoteMessage;") \
   METHOD (constructor,    "<init>",         "(Ljava/lang/String;)V") \
   METHOD (setCollapseKey, "setCollapseKey", "(Ljava/lang/String;)Lcom/google/firebase/messaging/RemoteMessage$Builder;") \
   METHOD (setMessageId,   "setMessageId",   "(Ljava/lang/String;)Lcom/google/firebase/messaging/RemoteMessage$Builder;") \
   METHOD (setMessageType, "setMessageType", "(Ljava/lang/String;)Lcom/google/firebase/messaging/RemoteMessage$Builder;") \
   METHOD (setTtl,         "setTtl",         "(I)Lcom/google/firebase/messaging/RemoteMessage$Builder;")

 DECLARE_JNI_CLASS (RemoteMessageBuilder, "com/google/firebase/messaging/RemoteMessage$Builder")
 #undef JNI_CLASS_MEMBERS

 #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
   METHOD (getBody,                  "getBody",                  "()Ljava/lang/String;") \
   METHOD (getBodyLocalizationArgs,  "getBodyLocalizationArgs",  "()[Ljava/lang/String;") \
   METHOD (getBodyLocalizationKey,   "getBodyLocalizationKey",   "()Ljava/lang/String;") \
   METHOD (getClickAction,           "getClickAction",           "()Ljava/lang/String;") \
   METHOD (getColor,                 "getColor",                 "()Ljava/lang/String;") \
   METHOD (getIcon,                  "getIcon",                  "()Ljava/lang/String;") \
   METHOD (getLink,                  "getLink",                  "()Landroid/net/Uri;") \
   METHOD (getSound,                 "getSound",                 "()Ljava/lang/String;") \
   METHOD (getTag,                   "getTag",                   "()Ljava/lang/String;") \
   METHOD (getTitle,                 "getTitle",                 "()Ljava/lang/String;") \
   METHOD (getTitleLocalizationArgs, "getTitleLocalizationArgs", "()[Ljava/lang/String;") \
   METHOD (getTitleLocalizationKey,  "getTitleLocalizationKey",  "()Ljava/lang/String;")

 DECLARE_JNI_CLASS (RemoteMessageNotification, "com/google/firebase/messaging/RemoteMessage$Notification")
 #undef JNI_CLASS_MEMBERS
#endif

//==============================================================================
bool PushNotifications::Notification::isValid() const noexcept
{
    bool isValidForPreApi26 = title.isNotEmpty() && body.isNotEmpty() && identifier.isNotEmpty() && icon.isNotEmpty();
    bool apiAtLeast26 = (getAndroidSDKVersion() >= 26);

    if (apiAtLeast26)
        return isValidForPreApi26 && channelId.isNotEmpty();

    return isValidForPreApi26;
}

//==============================================================================
struct PushNotifications::Pimpl
{
    Pimpl (PushNotifications& p)
        : owner (p)
    {}

    bool areNotificationsEnabled() const
    {
        if (getAndroidSDKVersion() >= 24)
        {
            auto* env = getEnv();

            auto notificationManager = getNotificationManager();

            if (notificationManager.get() != nullptr)
                return env->CallBooleanMethod (notificationManager, NotificationManagerApi24.areNotificationsEnabled);
        }

        return true;
    }

    //==============================================================================
    void sendLocalNotification (const PushNotifications::Notification& n)
    {
        // All required fields have to be setup!
        jassert (n.isValid());

        auto* env = getEnv();

        auto notificationManager = getNotificationManager();

        if (notificationManager.get() != nullptr)
        {
            auto notification = juceNotificationToJavaNotification (n);

            auto tag = javaString (n.identifier);
            const int id = 0;

            env->CallVoidMethod (notificationManager.get(), NotificationManagerBase.notify, tag.get(), id, notification.get());
        }
    }

    void getDeliveredNotifications() const
    {
        if (getAndroidSDKVersion() >= 23)
        {
            auto* env = getEnv();

            Array<PushNotifications::Notification> notifications;

            auto notificationManager = getNotificationManager();
            jassert (notificationManager != nullptr);

            if (notificationManager.get() != nullptr)
            {
                auto statusBarNotifications = LocalRef<jobjectArray> ((jobjectArray)env->CallObjectMethod (notificationManager,
                                                                                                           NotificationManagerApi23.getActiveNotifications));

                const int numNotifications = env->GetArrayLength (statusBarNotifications.get());

                for (int i = 0; i < numNotifications; ++i)
                {
                    auto statusBarNotification = LocalRef<jobject> (env->GetObjectArrayElement (statusBarNotifications.get(), (jsize) i));
                    auto notification = LocalRef<jobject> (env->CallObjectMethod (statusBarNotification, StatusBarNotification.getNotification));

                    notifications.add (javaNotificationToJuceNotification (notification));
                }
            }

            owner.listeners.call ([&] (Listener& l) { l.deliveredNotificationsListReceived (notifications); });
        }
        else
        {
            // Not supported on this platform
            jassertfalse;
            owner.listeners.call ([] (Listener& l) { l.deliveredNotificationsListReceived ({}); });
        }
    }

    void notifyListenersAboutLocalNotification (const LocalRef<jobject>& intent)
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        auto bundle = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getExtras));

        const auto notification = localNotificationBundleToJuceNotification (bundle);

        auto packageName  = juceString ((jstring) env->CallObjectMethod (context.get(), AndroidContext.getPackageName));

        String notificationString                = packageName + ".JUCE_NOTIFICATION.";
        String notificationButtonActionString    = packageName + ".JUCE_NOTIFICATION_BUTTON_ACTION.";
        String notificationTextInputActionString = packageName + ".JUCE_NOTIFICATION_TEXT_INPUT_ACTION.";

        auto actionString = juceString ((jstring) env->CallObjectMethod (intent, AndroidIntent.getAction));

        if (actionString.contains (notificationString))
        {
            owner.listeners.call ([&] (Listener& l) { l.handleNotification (true, notification); });
        }
        else if (actionString.contains (notificationButtonActionString))
        {
            auto prefix = notificationButtonActionString + notification.identifier + ".";

            auto actionTitle = actionString.fromLastOccurrenceOf (prefix, false, false)     // skip prefix
                                           .fromFirstOccurrenceOf (".", false, false);      // skip action index

            owner.listeners.call ([&] (Listener& l) { l.handleNotificationAction (true, notification, actionTitle, {}); });
        }
        else if (getAndroidSDKVersion() >= 20 && actionString.contains (notificationTextInputActionString))
        {
            auto prefix = notificationTextInputActionString + notification.identifier + ".";

            auto actionTitle = actionString.fromLastOccurrenceOf (prefix, false, false)     // skip prefix
                                           .fromFirstOccurrenceOf (".", false, false);      // skip action index

            auto actionIndex = actionString.fromLastOccurrenceOf (prefix, false, false).upToFirstOccurrenceOf (".", false, false);
            auto resultKeyString = javaString (actionTitle + actionIndex);

            auto remoteInputResult = LocalRef<jobject> (env->CallStaticObjectMethod (RemoteInput, RemoteInput.getResultsFromIntent, intent.get()));
            String responseString;

            if (remoteInputResult.get() == nullptr)
            {
                auto charSequence      = LocalRef<jobject> (env->CallObjectMethod (remoteInputResult, AndroidBundle.getCharSequence, resultKeyString.get()));
                auto responseStringRef = LocalRef<jstring> ((jstring) env->CallObjectMethod (charSequence, JavaCharSequence.toString));
                responseString = juceString (responseStringRef.get());
            }

            owner.listeners.call ([&] (Listener& l) { l.handleNotificationAction (true, notification, actionTitle, responseString); });
        }
    }

    void notifyListenersAboutLocalNotificationDeleted (const LocalRef<jobject>& intent)
    {
        auto* env = getEnv();

        auto bundle = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getExtras));
        auto notification = localNotificationBundleToJuceNotification (bundle);

        owner.listeners.call ([&] (Listener& l) { l.localNotificationDismissedByUser (notification); });
    }

    void removeAllDeliveredNotifications()
    {
        auto* env = getEnv();

        auto notificationManager = getNotificationManager();

        if (notificationManager.get() != nullptr)
            env->CallVoidMethod (notificationManager.get(), NotificationManagerBase.cancelAll);
    }

    void removeDeliveredNotification (const String& identifier)
    {
        auto* env = getEnv();

        auto notificationManager = getNotificationManager();

        if (notificationManager.get() != nullptr)
        {
            auto tag = javaString (identifier);
            const int id = 0;

            env->CallVoidMethod (notificationManager.get(), NotificationManagerBase.cancel, tag.get(), id);
        }
    }

    //==============================================================================
    String getDeviceToken() const
    {
      #if defined(JUCE_FIREBASE_INSTANCE_ID_SERVICE_CLASSNAME)
        auto* env = getEnv();

        auto instanceId = LocalRef<jobject> (env->CallStaticObjectMethod (FirebaseInstanceId, FirebaseInstanceId.getInstance));

        return juceString ((jstring) env->CallObjectMethod (instanceId, FirebaseInstanceId.getToken));
      #else
        return {};
      #endif
    }

    void notifyListenersTokenRefreshed (const String& token)
    {
      #if defined(JUCE_FIREBASE_INSTANCE_ID_SERVICE_CLASSNAME)
        MessageManager::callAsync ([this, token]
        {
            owner.listeners.call ([&] (Listener& l) { l.deviceTokenRefreshed (token); });
        });
      #else
        ignoreUnused (token);
      #endif
    }

    //==============================================================================
    void subscribeToTopic (const String& topic)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        auto* env = getEnv();

        auto firebaseMessaging = LocalRef<jobject> (env->CallStaticObjectMethod (FirebaseMessaging,
                                                                                 FirebaseMessaging.getInstance));

        env->CallObjectMethod (firebaseMessaging, FirebaseMessaging.subscribeToTopic, javaString (topic).get());
      #else
        ignoreUnused (topic);
      #endif
    }

    void unsubscribeFromTopic (const String& topic)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        auto* env = getEnv();

        auto firebaseMessaging = LocalRef<jobject> (env->CallStaticObjectMethod (FirebaseMessaging,
                                                                                 FirebaseMessaging.getInstance));

        env->CallObjectMethod (firebaseMessaging, FirebaseMessaging.unsubscribeFromTopic, javaString (topic).get());
      #else
        ignoreUnused (topic);
      #endif
    }

    void sendUpstreamMessage (const String& serverSenderId,
                              const String& collapseKey,
                              const String& messageId,
                              const String& messageType,
                              int timeToLive,
                              const StringPairArray& additionalData)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        auto* env = getEnv();

        auto messageBuilder = LocalRef<jobject> (env->NewObject (RemoteMessageBuilder,
                                                                 RemoteMessageBuilder.constructor,
                                                                 javaString (serverSenderId + "@gcm_googleapis.com").get()));

        env->CallObjectMethod (messageBuilder, RemoteMessageBuilder.setCollapseKey, javaString (collapseKey).get());
        env->CallObjectMethod (messageBuilder, RemoteMessageBuilder.setMessageId, javaString (messageId).get());
        env->CallObjectMethod (messageBuilder, RemoteMessageBuilder.setMessageType, javaString (messageType).get());
        env->CallObjectMethod (messageBuilder, RemoteMessageBuilder.setTtl, timeToLive);

        auto keys = additionalData.getAllKeys();

        for (const auto& key : keys)
            env->CallObjectMethod (messageBuilder,
                                   RemoteMessageBuilder.addData,
                                   javaString (key).get(),
                                   javaString (additionalData[key]).get());

        auto message = LocalRef<jobject> (env->CallObjectMethod (messageBuilder, RemoteMessageBuilder.build));

        auto firebaseMessaging = LocalRef<jobject> (env->CallStaticObjectMethod (FirebaseMessaging,
                                                                                 FirebaseMessaging.getInstance));

        env->CallVoidMethod (firebaseMessaging, FirebaseMessaging.send, message.get());
      #else
        ignoreUnused (serverSenderId, collapseKey, messageId, messageType);
        ignoreUnused (timeToLive, additionalData);
      #endif
    }

    void notifyListenersAboutRemoteNotificationFromSystemTray (const LocalRef<jobject>& intent)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        auto* env = getEnv();

        auto bundle = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getExtras));
        auto notification = remoteNotificationBundleToJuceNotification (bundle);

        owner.listeners.call ([&] (Listener& l) { l.handleNotification (false, notification); });
      #else
        ignoreUnused (intent);
      #endif
    }

    void notifyListenersAboutRemoteNotificationFromService (const LocalRef<jobject>& remoteNotification)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        GlobalRef rn (remoteNotification);

        MessageManager::callAsync ([this, rn]
        {
            auto notification = firebaseRemoteNotificationToJuceNotification (rn.get());
            owner.listeners.call ([&] (Listener& l) { l.handleNotification (false, notification); });
        });
      #else
        ignoreUnused (remoteNotification);
      #endif
    }

    void notifyListenersAboutRemoteNotificationsDeleted()
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        MessageManager::callAsync ([this]
        {
            owner.listeners.call ([] (Listener& l) { l.remoteNotificationsDeleted(); });
        });
      #endif
    }

    void notifyListenersAboutUpstreamMessageSent (const LocalRef<jstring>& messageId)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        GlobalRef mid (LocalRef<jobject>(messageId.get()));

        MessageManager::callAsync ([this, mid]
        {
            auto midString = juceString ((jstring) mid.get());
            owner.listeners.call ([&] (Listener& l) { l.upstreamMessageSent (midString); });
        });
      #else
        ignoreUnused (messageId);
      #endif
    }

    void notifyListenersAboutUpstreamMessageSendingError (const LocalRef<jstring>& messageId,
                                                          const LocalRef<jstring>& error)
    {
      #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
        GlobalRef mid (LocalRef<jobject>(messageId.get())), e (LocalRef<jobject>(error.get()));

        MessageManager::callAsync ([this, mid, e]
        {
            auto midString = juceString ((jstring) mid.get());
            auto eString   = juceString ((jstring) e.get());

            owner.listeners.call ([&] (Listener& l) { l.upstreamMessageSendingError (midString, eString); });
        });
      #else
        ignoreUnused (messageId, error);
      #endif
    }

    static LocalRef<jobject> getNotificationManager()
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        return LocalRef<jobject> (env->CallObjectMethod (context.get(),
                                                         AndroidContext.getSystemService,
                                                         javaString ("notification").get()));
    }

    static LocalRef<jobject> juceNotificationToJavaNotification (const PushNotifications::Notification& n)
    {
        auto* env = getEnv();

        auto notificationBuilder = createNotificationBuilder (n);

        setupRequiredFields (n, notificationBuilder);
        setupOptionalFields (n, notificationBuilder);

        if (n.actions.size() > 0)
            setupActions (n, notificationBuilder);

        if (getAndroidSDKVersion() >= 16)
            return LocalRef<jobject> (env->CallObjectMethod (notificationBuilder, NotificationBuilderApi16.build));

        return LocalRef<jobject> (env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.getNotification));
    }

    static LocalRef<jobject> createNotificationBuilder (const PushNotifications::Notification& n)
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        jclass builderClass = env->FindClass ("android/app/Notification$Builder");
        jassert (builderClass != nullptr);

        if (builderClass == nullptr)
            return LocalRef<jobject> (nullptr);

        jmethodID builderConstructor = nullptr;

        const bool apiAtLeast26 = (getAndroidSDKVersion() >= 26);

        if (apiAtLeast26)
            builderConstructor = env->GetMethodID (builderClass, "<init>", "(Landroid/content/Context;Ljava/lang/String;)V");
        else
            builderConstructor = env->GetMethodID (builderClass, "<init>", "(Landroid/content/Context;)V");

        jassert (builderConstructor != nullptr);

        if (builderConstructor == nullptr)
            return LocalRef<jobject> (nullptr);

        if (apiAtLeast26)
            return LocalRef<jobject> (env->NewObject (builderClass, builderConstructor,
                                                      context.get(), javaString (n.channelId).get()));

        return LocalRef<jobject> (env->NewObject (builderClass, builderConstructor, context.get()));
    }

    static void setupRequiredFields (const PushNotifications::Notification& n, LocalRef<jobject>& notificationBuilder)
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        auto activityClass = LocalRef<jobject> (env->CallObjectMethod (context.get(), JavaObject.getClass));
        auto notifyIntent  = LocalRef<jobject> (env->NewObject (AndroidIntent, AndroidIntent.constructorWithContextAndClass, context.get(), activityClass.get()));

        auto packageNameString  = LocalRef<jstring> ((jstring) (env->CallObjectMethod (context.get(), AndroidContext.getPackageName)));
        auto actionStringSuffix = javaString (".JUCE_NOTIFICATION." + n.identifier);
        auto actionString       = LocalRef<jstring> ((jstring)env->CallObjectMethod (packageNameString, JavaString.concat, actionStringSuffix.get()));

        env->CallObjectMethod (notifyIntent, AndroidIntent.setAction, actionString.get());
        // Packaging entire notification into extras bundle here, so that we can retrieve all the details later on
        env->CallObjectMethod (notifyIntent, AndroidIntent.putExtras, juceNotificationToBundle (n).get());

        auto notifyPendingIntent = LocalRef<jobject> (env->CallStaticObjectMethod (AndroidPendingIntent,
                                                                                   AndroidPendingIntent.getActivity,
                                                                                   context.get(),
                                                                                   1002,
                                                                                   notifyIntent.get(),
                                                                                   0));

        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setContentTitle,  javaString (n.title).get());
        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setContentText,   javaString (n.body).get());
        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setContentIntent, notifyPendingIntent.get());

        auto resources = LocalRef<jobject> (env->CallObjectMethod (context.get(), AndroidContext.getResources));
        const int iconId = env->CallIntMethod (resources, AndroidResources.getIdentifier, javaString (n.icon).get(),
                                               javaString ("raw").get(), packageNameString.get());

        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setSmallIcon, iconId);

        if (getAndroidSDKVersion() >= 21 && n.publicVersion != nullptr)
        {
            // Public version of a notification is not expected to have another public one!
            jassert (n.publicVersion->publicVersion == nullptr);

            auto publicNotificationBuilder = createNotificationBuilder (n);

            setupRequiredFields (*n.publicVersion, publicNotificationBuilder);
            setupOptionalFields (*n.publicVersion, publicNotificationBuilder);

            auto publicVersion = LocalRef<jobject> (env->CallObjectMethod (publicNotificationBuilder, NotificationBuilderApi16.build));
            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi21.setPublicVersion, publicVersion.get());
        }
    }

    static LocalRef<jobject> juceNotificationToBundle (const PushNotifications::Notification& n)
    {
        auto* env = getEnv();

        auto bundle = LocalRef<jobject> (env->NewObject (AndroidBundle, AndroidBundle.constructor));

        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("identifier")              .get(), javaString (n.identifier).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("title")                   .get(), javaString (n.title).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("body")                    .get(), javaString (n.body).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("subtitle")                .get(), javaString (n.subtitle).get());
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("badgeNumber")             .get(), n.badgeNumber);
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("soundToPlay")             .get(), javaString (n.soundToPlay.toString (true)).get());
        env->CallVoidMethod (bundle, AndroidBundle.putBundle,   javaString ("properties")              .get(), varToBundleWithPropertiesString (n.properties).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("icon")                    .get(), javaString (n.icon).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("channelId")               .get(), javaString (n.channelId).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("tickerText")              .get(), javaString (n.tickerText).get());
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("progressMax")             .get(), n.progress.max);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("progressCurrent")         .get(), n.progress.current);
        env->CallVoidMethod (bundle, AndroidBundle.putBoolean,  javaString ("progressIndeterminate")   .get(), n.progress.indeterminate);
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("person")                  .get(), javaString (n.person).get());
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("type")                    .get(), n.type);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("priority")                .get(), n.priority);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("lockScreenAppearance")    .get(), n.lockScreenAppearance);
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("groupId")                 .get(), javaString (n.groupId).get());
        env->CallVoidMethod (bundle, AndroidBundle.putString,   javaString ("groupSortKey")            .get(), javaString (n.groupSortKey).get());
        env->CallVoidMethod (bundle, AndroidBundle.putBoolean,  javaString ("groupSummary")            .get(), n.groupSummary);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("accentColour")            .get(), n.accentColour.getARGB());
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("ledColour")               .get(), n.ledColour.getARGB());
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("ledBlinkPatternMsToBeOn") .get(), n.ledBlinkPattern.msToBeOn);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("ledBlinkPatternMsToBeOff").get(), n.ledBlinkPattern.msToBeOff);
        env->CallVoidMethod (bundle, AndroidBundle.putBoolean,  javaString ("shouldAutoCancel")        .get(), n.shouldAutoCancel);
        env->CallVoidMethod (bundle, AndroidBundle.putBoolean,  javaString ("localOnly")               .get(), n.localOnly);
        env->CallVoidMethod (bundle, AndroidBundle.putBoolean,  javaString ("ongoing")                 .get(), n.ongoing);
        env->CallVoidMethod (bundle, AndroidBundle.putBoolean,  javaString ("alertOnlyOnce")           .get(), n.alertOnlyOnce);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("timestampVisibility")     .get(), n.timestampVisibility);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("badgeIconType")           .get(), n.badgeIconType);
        env->CallVoidMethod (bundle, AndroidBundle.putInt,      javaString ("groupAlertBehaviour")     .get(), n.groupAlertBehaviour);
        env->CallVoidMethod (bundle, AndroidBundle.putLong,     javaString ("timeoutAfterMs")          .get(), (jlong)n.timeoutAfterMs);

        const int size = n.vibrationPattern.size();

        if (size > 0)
        {
            auto array = LocalRef<jlongArray> (env->NewLongArray (size));

            jlong* elements = env->GetLongArrayElements (array, nullptr);

            for (int i = 0; i < size; ++i)
                elements[i] = (jlong) n.vibrationPattern[i];

            env->SetLongArrayRegion (array, 0, size, elements);
            env->CallVoidMethod (bundle, AndroidBundle.putLongArray, javaString ("vibrationPattern").get(), array.get());
        }

        return bundle;
    }

    static void setupOptionalFields (const PushNotifications::Notification& n, LocalRef<jobject>& notificationBuilder)
    {
        auto* env = getEnv();

        if (n.subtitle.isNotEmpty())
            env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setContentInfo, javaString (n.subtitle).get());

        auto soundName = n.soundToPlay.toString (true);

        if (soundName == "default_os_sound")
        {
            const int playDefaultSound = 1;
            env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setDefaults, playDefaultSound);
        }
        else if (! soundName.isEmpty())
        {
            env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setSound, juceUrlToAndroidUri (n.soundToPlay).get());
        }

        if (n.largeIcon.isValid())
            env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setLargeIcon, imagetoJavaBitmap (n.largeIcon).get());

        if (n.tickerText.isNotEmpty())
            env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setTicker, javaString (n.tickerText).get());

        if (n.ledColour != Colour())
        {
            env->CallObjectMethod (notificationBuilder,
                                   NotificationBuilderBase.setLights,
                                   n.ledColour.getARGB(),
                                   n.ledBlinkPattern.msToBeOn,
                                   n.ledBlinkPattern.msToBeOff);
        }

        if (! n.vibrationPattern.isEmpty())
        {
            const int size = n.vibrationPattern.size();

            if (size > 0)
            {
                auto array = LocalRef<jlongArray> (env->NewLongArray (size));

                jlong* elements = env->GetLongArrayElements (array, nullptr);

                for (int i = 0; i < size; ++i)
                    elements[i] = (jlong) n.vibrationPattern[i];

                env->SetLongArrayRegion (array, 0, size, elements);
                env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setVibrate, array.get());
            }
        }

        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setProgress, n.progress.max, n.progress.current, n.progress.indeterminate);
        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setNumber, n.badgeNumber);
        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setAutoCancel, n.shouldAutoCancel);
        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setOngoing, n.ongoing);
        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setOnlyAlertOnce, n.alertOnlyOnce);

        if (getAndroidSDKVersion() >= 16)
        {
            if (n.subtitle.isNotEmpty())
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi16.setSubText, javaString (n.subtitle).get());

            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi16.setPriority, n.priority);

            if (getAndroidSDKVersion() < 24)
            {
                const bool useChronometer = n.timestampVisibility == PushNotifications::Notification::chronometer;
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi16.setUsesChronometer, useChronometer);
            }
        }

        if (getAndroidSDKVersion() >= 17)
        {
            const bool showTimeStamp = n.timestampVisibility != PushNotifications::Notification::off;
            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi17.setShowWhen, showTimeStamp);
        }

        if (getAndroidSDKVersion() >= 20)
        {
            if (n.groupId.isNotEmpty())
            {
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi20.setGroup, javaString (n.groupId).get());
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi20.setGroupSummary, n.groupSummary);
            }

            if (n.groupSortKey.isNotEmpty())
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi20.setSortKey, javaString (n.groupSortKey).get());

            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi20.setLocalOnly, n.localOnly);

            auto extras = LocalRef<jobject> (env->NewObject (AndroidBundle, AndroidBundle.constructor));

            env->CallVoidMethod (extras, AndroidBundle.putBundle, javaString ("notificationData").get(),
                                 juceNotificationToBundle (n).get());

            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi20.addExtras, extras.get());
        }

        if (getAndroidSDKVersion() >= 21)
        {
            if (n.person.isNotEmpty())
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi21.addPerson, javaString (n.person).get());

            auto categoryString = typeToCategory (n.type);
            if (categoryString.isNotEmpty())
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi21.setCategory, javaString (categoryString).get());

            if (n.accentColour != Colour())
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi21.setColor, n.accentColour.getARGB());

            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi21.setVisibility, n.lockScreenAppearance);
        }

        if (getAndroidSDKVersion() >= 24)
        {
            const bool useChronometer = n.timestampVisibility == PushNotifications::Notification::chronometer;
            const bool useCountDownChronometer = n.timestampVisibility == PushNotifications::Notification::countDownChronometer;

            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi24.setChronometerCountDown, useCountDownChronometer);
            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi16.setUsesChronometer, useChronometer | useCountDownChronometer);
        }

        if (getAndroidSDKVersion() >= 26)
        {
            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi26.setBadgeIconType, n.badgeIconType);
            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi26.setGroupAlertBehavior, n.groupAlertBehaviour);
            env->CallObjectMethod (notificationBuilder, NotificationBuilderApi26.setTimeoutAfter, (jlong) n.timeoutAfterMs);
        }

        setupNotificationDeletedCallback (n, notificationBuilder);
    }

    static void setupNotificationDeletedCallback (const PushNotifications::Notification& n,
                                                  LocalRef<jobject>& notificationBuilder)
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        auto activityClass = LocalRef<jobject> (env->CallObjectMethod (context.get(), JavaObject.getClass));
        auto deleteIntent  = LocalRef<jobject> (env->NewObject (AndroidIntent, AndroidIntent.constructorWithContextAndClass, context.get(), activityClass.get()));

        auto packageNameString  = LocalRef<jstring> ((jstring) (env->CallObjectMethod (context.get(), AndroidContext.getPackageName)));
        auto actionStringSuffix = javaString (".JUCE_NOTIFICATION_DELETED." + n.identifier);
        auto actionString       = LocalRef<jstring> ((jstring)env->CallObjectMethod (packageNameString, JavaString.concat, actionStringSuffix.get()));

        env->CallObjectMethod (deleteIntent, AndroidIntent.setAction, actionString.get());
        env->CallObjectMethod (deleteIntent, AndroidIntent.putExtras, juceNotificationToBundle (n).get());

        auto deletePendingIntent = LocalRef<jobject> (env->CallStaticObjectMethod (AndroidPendingIntent,
                                                                                   AndroidPendingIntent.getActivity,
                                                                                   context.get(),
                                                                                   1002,
                                                                                   deleteIntent.get(),
                                                                                   0));

        env->CallObjectMethod (notificationBuilder, NotificationBuilderBase.setDeleteIntent, deletePendingIntent.get());
    }

    static void setupActions (const PushNotifications::Notification& n, LocalRef<jobject>& notificationBuilder)
    {
        if (getAndroidSDKVersion() < 16)
            return;

        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        int actionIndex = 0;

        for (const auto& action : n.actions)
        {
            auto activityClass = LocalRef<jobject> (env->CallObjectMethod (context.get(), JavaObject.getClass));
            auto notifyIntent  = LocalRef<jobject> (env->NewObject (AndroidIntent, AndroidIntent.constructorWithContextAndClass, context.get(), activityClass.get()));

            const bool isTextStyle = action.style == PushNotifications::Notification::Action::text;

            auto packageNameString   = LocalRef<jstring> ((jstring) (env->CallObjectMethod (context.get(), AndroidContext.getPackageName)));
            const String notificationActionString = isTextStyle ? ".JUCE_NOTIFICATION_TEXT_INPUT_ACTION." : ".JUCE_NOTIFICATION_BUTTON_ACTION.";
            auto actionStringSuffix  = javaString (notificationActionString + n.identifier + "." + String (actionIndex) + "." + action.title);
            auto actionString        = LocalRef<jstring> ((jstring)env->CallObjectMethod (packageNameString, JavaString.concat, actionStringSuffix.get()));

            env->CallObjectMethod (notifyIntent, AndroidIntent.setAction, actionString.get());
            // Packaging entire notification into extras bundle here, so that we can retrieve all the details later on
            env->CallObjectMethod (notifyIntent, AndroidIntent.putExtras, juceNotificationToBundle (n).get());

            auto notifyPendingIntent = LocalRef<jobject> (env->CallStaticObjectMethod (AndroidPendingIntent,
                                                                                       AndroidPendingIntent.getActivity,
                                                                                       context.get(),
                                                                                       1002,
                                                                                       notifyIntent.get(),
                                                                                       0));

            auto resources = LocalRef<jobject> (env->CallObjectMethod (context.get(), AndroidContext.getResources));
            int iconId = env->CallIntMethod (resources, AndroidResources.getIdentifier, javaString (action.icon).get(),
                                             javaString ("raw").get(), packageNameString.get());

            if (iconId == 0)
                iconId = env->CallIntMethod (resources, AndroidResources.getIdentifier, javaString (n.icon).get(),
                                             javaString ("raw").get(), packageNameString.get());

            if (getAndroidSDKVersion() >= 20)
            {
                auto actionBuilder = LocalRef<jobject> (env->NewObject (NotificationActionBuilder,
                                                                        NotificationActionBuilder.constructor,
                                                                        iconId,
                                                                        javaString (action.title).get(),
                                                                        notifyPendingIntent.get()));

                env->CallObjectMethod (actionBuilder, NotificationActionBuilder.addExtras,
                                       varToBundleWithPropertiesString (action.parameters).get());

                if (isTextStyle)
                {
                    auto resultKey = javaString (action.title + String (actionIndex));
                    auto remoteInputBuilder = LocalRef<jobject> (env->NewObject (RemoteInputBuilder,
                                                                                 RemoteInputBuilder.constructor,
                                                                                 resultKey.get()));

                    if (! action.textInputPlaceholder.isEmpty())
                        env->CallObjectMethod (remoteInputBuilder, RemoteInputBuilder.setLabel, javaString (action.textInputPlaceholder).get());

                    if (! action.allowedResponses.isEmpty())
                    {
                        env->CallObjectMethod (remoteInputBuilder, RemoteInputBuilder.setAllowFreeFormInput, false);

                        const int size = action.allowedResponses.size();

                        auto array = LocalRef<jobjectArray> (env->NewObjectArray (size, env->FindClass ("java/lang/String"), nullptr));

                        for (int i = 0; i < size; ++i)
                        {
                            const auto& response = action.allowedResponses[i];
                            auto responseString = javaString (response);

                            env->SetObjectArrayElement (array, i, responseString.get());
                        }

                        env->CallObjectMethod (remoteInputBuilder, RemoteInputBuilder.setChoices, array.get());
                    }

                    env->CallObjectMethod (actionBuilder, NotificationActionBuilder.addRemoteInput,
                                           env->CallObjectMethod (remoteInputBuilder, RemoteInputBuilder.build));
                }

                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi20.addAction,
                                       env->CallObjectMethod (actionBuilder, NotificationActionBuilder.build));
            }
            else
            {
                env->CallObjectMethod (notificationBuilder, NotificationBuilderApi16.addAction,
                                       iconId, javaString (action.title).get(), notifyPendingIntent.get());
            }

            ++actionIndex;
        }
    }

    static LocalRef<jobject> juceUrlToAndroidUri (const URL& url)
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        auto packageNameString = LocalRef<jstring> ((jstring) (env->CallObjectMethod (context.get(), AndroidContext.getPackageName)));

        auto resources = LocalRef<jobject> (env->CallObjectMethod (context.get(), AndroidContext.getResources));
        const int id = env->CallIntMethod (resources, AndroidResources.getIdentifier, javaString (url.toString (true)).get(),
                                           javaString ("raw").get(), packageNameString.get());

        auto schemeString   = javaString ("android.resource://");
        auto resourceString = javaString ("/" + String (id));
        auto uriString = LocalRef<jstring> ((jstring) env->CallObjectMethod (schemeString, JavaString.concat, packageNameString.get()));
        uriString = LocalRef<jstring> ((jstring) env->CallObjectMethod (uriString, JavaString.concat, resourceString.get()));

        return LocalRef<jobject> (env->CallStaticObjectMethod (AndroidUri, AndroidUri.parse, uriString.get()));
    }

    static LocalRef<jobject> imagetoJavaBitmap (const Image& image)
    {
        auto* env = getEnv();

        Image imageToUse = image.convertedToFormat (Image::PixelFormat::ARGB);

        auto bitmapConfig = LocalRef<jobject> (env->CallStaticObjectMethod (AndroidBitmapConfig,
                                                                            AndroidBitmapConfig.valueOf,
                                                                            javaString ("ARGB_8888").get()));

        auto bitmap = LocalRef<jobject> (env->CallStaticObjectMethod (AndroidBitmap,
                                                                      AndroidBitmap.createBitmap,
                                                                      image.getWidth(),
                                                                      image.getHeight(),
                                                                      bitmapConfig.get()));

        for (int i = 0; i < image.getWidth(); ++i)
            for (int j = 0; j < image.getHeight(); ++j)
                env->CallVoidMethod (bitmap.get(), AndroidBitmap.setPixel, i, j, image.getPixelAt (i, j).getARGB());

        return bitmap;
    }

    static String typeToCategory (PushNotifications::Notification::Type t)
    {
        switch (t)
        {
            case PushNotifications::Notification::unspecified:    return {};
            case PushNotifications::Notification::alarm:          return "alarm";
            case PushNotifications::Notification::call:           return "call";
            case PushNotifications::Notification::email:          return "email";
            case PushNotifications::Notification::error:          return "err";
            case PushNotifications::Notification::event:          return "event";
            case PushNotifications::Notification::message:        return "msg";
            case PushNotifications::Notification::taskProgress:   return "progress";
            case PushNotifications::Notification::promo:          return "promo";
            case PushNotifications::Notification::recommendation: return "recommendation";
            case PushNotifications::Notification::reminder:       return "reminder";
            case PushNotifications::Notification::service:        return "service";
            case PushNotifications::Notification::social:         return "social";
            case PushNotifications::Notification::status:         return "status";
            case PushNotifications::Notification::system:         return "sys";
            case PushNotifications::Notification::transport:      return "transport";
        }

        return {};
    }

    static LocalRef<jobject> varToBundleWithPropertiesString (const var& varToParse)
    {
        auto* env = getEnv();

        auto bundle = LocalRef<jobject> (env->NewObject (AndroidBundle, AndroidBundle.constructor));
        env->CallVoidMethod (bundle, AndroidBundle.putString, javaString ("properties").get(),
                             javaString (JSON::toString (varToParse, false)).get());

        return bundle;
    }

    // Gets "properties" var from bundle.
    static var bundleWithPropertiesStringToVar (const LocalRef<jobject>& bundle)
    {
        auto* env = getEnv();

        auto varString = LocalRef<jstring> ((jstring)env->CallObjectMethod (bundle, AndroidBundle.getString,
                                                                            javaString ("properties").get()));

        var resultVar;
        JSON::parse (juceString (varString.get()), resultVar);

        // Note: We are not checking if result of parsing was okay, because there may be no properties set at all.
        return resultVar;
    }

    // Reverse of juceNotificationToBundle().
    static PushNotifications::Notification localNotificationBundleToJuceNotification (const LocalRef<jobject>& bundle)
    {
        auto* env = getEnv();

        PushNotifications::Notification n;

        if (bundle.get() != nullptr)
        {
            n.identifier  = getStringFromBundle (env, "identifier", bundle);
            n.title       = getStringFromBundle (env, "title", bundle);
            n.body        = getStringFromBundle (env, "body", bundle);
            n.subtitle    = getStringFromBundle (env, "subtitle", bundle);
            n.badgeNumber = getIntFromBundle    (env, "badgeNumber", bundle);
            n.soundToPlay = URL (getStringFromBundle (env, "soundToPlay", bundle));
            n.properties  = getPropertiesVarFromBundle (env, "properties", bundle);
            n.tickerText  = getStringFromBundle (env, "tickerText", bundle);
            n.icon        = getStringFromBundle (env, "icon", bundle);
            n.channelId   = getStringFromBundle (env, "channelId", bundle);

            PushNotifications::Notification::Progress progress;
            progress.max           = getIntFromBundle  (env, "progressMax", bundle);
            progress.current       = getIntFromBundle  (env, "progressCurrent", bundle);
            progress.indeterminate = getBoolFromBundle (env, "progressIndeterminate", bundle);
            n.progress = progress;

            n.person       = getStringFromBundle (env, "person", bundle);
            n.type         = (PushNotifications::Notification::Type)     getIntFromBundle (env, "type", bundle);
            n.priority     = (PushNotifications::Notification::Priority) getIntFromBundle (env, "priority", bundle);
            n.lockScreenAppearance = (PushNotifications::Notification::LockScreenAppearance) getIntFromBundle (env, "lockScreenAppearance", bundle);
            n.groupId      = getStringFromBundle (env, "groupId", bundle);
            n.groupSortKey = getStringFromBundle (env, "groupSortKey", bundle);
            n.groupSummary = getBoolFromBundle   (env, "groupSummary", bundle);
            n.accentColour = Colour ((uint32) getIntFromBundle (env, "accentColour", bundle));
            n.ledColour    = Colour ((uint32) getIntFromBundle (env, "ledColour", bundle));

            PushNotifications::Notification::LedBlinkPattern ledBlinkPattern;
            ledBlinkPattern.msToBeOn  = getIntFromBundle (env, "ledBlinkPatternMsToBeOn", bundle);
            ledBlinkPattern.msToBeOff = getIntFromBundle (env, "ledBlinkPatternMsToBeOff", bundle);
            n.ledBlinkPattern = ledBlinkPattern;

            n.vibrationPattern = getLongArrayFromBundle (env, "vibrationPattern", bundle);

            n.shouldAutoCancel    = getBoolFromBundle (env, "shouldAutoCancel", bundle);
            n.localOnly           = getBoolFromBundle (env, "localOnly", bundle);
            n.ongoing             = getBoolFromBundle (env, "ongoing", bundle);
            n.alertOnlyOnce       = getBoolFromBundle (env, "alertOnlyOnce", bundle);
            n.timestampVisibility = (PushNotifications::Notification::TimestampVisibility) getIntFromBundle (env, "timestampVisibility", bundle);
            n.badgeIconType       = (PushNotifications::Notification::BadgeIconType) getIntFromBundle (env, "badgeIconType", bundle);
            n.groupAlertBehaviour = (PushNotifications::Notification::GroupAlertBehaviour) getIntFromBundle (env, "groupAlertBehaviour", bundle);
            n.timeoutAfterMs      = getLongFromBundle (env, "timeoutAfterMs", bundle);
        }

        return n;
    }

    static String getStringFromBundle (JNIEnv* env, const String& key, const LocalRef<jobject>& bundle)
    {
        auto keyString = javaString (key);

        if (env->CallBooleanMethod (bundle, AndroidBundle.containsKey, keyString.get()))
        {
            auto value = LocalRef<jstring> ((jstring)env->CallObjectMethod (bundle, AndroidBundle.getString, keyString.get()));
            return juceString (value);
        }

        return {};
    }

    static int getIntFromBundle (JNIEnv* env, const String& key, const LocalRef<jobject>& bundle)
    {
        auto keyString = javaString (key);

        if (env->CallBooleanMethod (bundle, AndroidBundle.containsKey, keyString.get()))
            return env->CallIntMethod (bundle, AndroidBundle.getInt, keyString.get());

        return 0;
    }

    // Converting to int on purpose!
    static int getLongFromBundle (JNIEnv* env, const String& key, const LocalRef<jobject>& bundle)
    {
        auto keyString = javaString (key);

        if (env->CallBooleanMethod (bundle, AndroidBundle.containsKey, keyString.get()))
            return (int) env->CallLongMethod (bundle, AndroidBundle.getLong, keyString.get());

        return 0;
    }

    static var getPropertiesVarFromBundle (JNIEnv* env, const String& key, const LocalRef<jobject>& bundle)
    {
        auto keyString = javaString (key);

        if (env->CallBooleanMethod (bundle, AndroidBundle.containsKey, keyString.get()))
        {
            auto value = LocalRef<jobject> (env->CallObjectMethod (bundle, AndroidBundle.getBundle, keyString.get()));
            return bundleWithPropertiesStringToVar (value);
        }

        return {};
    }

    static bool getBoolFromBundle (JNIEnv* env, const String& key, const LocalRef<jobject>& bundle)
    {
        auto keyString = javaString (key);

        if (env->CallBooleanMethod (bundle, AndroidBundle.containsKey, keyString.get()))
            return env->CallBooleanMethod (bundle, AndroidBundle.getBoolean, keyString.get());

        return false;
    }

    static Array<int> getLongArrayFromBundle (JNIEnv* env, const String& key, const LocalRef<jobject>& bundle)
    {
        auto keyString = javaString (key);

        if (env->CallBooleanMethod (bundle, AndroidBundle.containsKey, keyString.get()))
        {
            auto array = LocalRef<jlongArray> ((jlongArray) env->CallObjectMethod (bundle, AndroidBundle.getLongArray, keyString.get()));

            const int size = env->GetArrayLength (array.get());

            jlong* elements = env->GetLongArrayElements (array.get(), nullptr);

            Array<int> resultArray;

            for (int i = 0; i < size; ++i)
                resultArray.add ((int) *elements++);

            return resultArray;
        }

        return {};
    }

    static PushNotifications::Notification javaNotificationToJuceNotification (const LocalRef<jobject>& notification)
    {
        if (getAndroidSDKVersion() < 20)
            return {};

        auto* env = getEnv();

        auto extras = LocalRef<jobject> (env->GetObjectField (notification, AndroidNotification.extras));
        auto notificationData = LocalRef<jobject> (env->CallObjectMethod (extras, AndroidBundle.getBundle,
                                                                          javaString ("notificationData").get()));

        if (notificationData.get() != nullptr)
            return localNotificationBundleToJuceNotification (notificationData);

        return remoteNotificationBundleToJuceNotification (extras);
    }

    static PushNotifications::Notification remoteNotificationBundleToJuceNotification (const LocalRef<jobject>& bundle)
    {
        // This will probably work only for remote notifications that get delivered to system tray
        PushNotifications::Notification n;
        n.properties = bundleToVar (bundle);

        return n;
    }

    static var bundleToVar (const LocalRef<jobject>& bundle)
    {
        if (bundle.get() == nullptr)
        {
            auto* env = getEnv();

            auto keySet   = LocalRef<jobject> (env->CallObjectMethod (bundle, AndroidBundle.keySet));
            auto iterator = LocalRef<jobject> (env->CallObjectMethod (keySet, JavaSet.iterator));

            DynamicObject::Ptr dynamicObject = new DynamicObject();

            for (;;)
            {
                if (! env->CallBooleanMethod (iterator, JavaIterator.hasNext))
                    break;

                auto key            = LocalRef<jstring> ((jstring) env->CallObjectMethod (iterator, JavaIterator.next));
                auto object         = LocalRef<jobject> (env->CallObjectMethod (bundle, AndroidBundle.get, key.get()));

                if (object.get() != nullptr)
                {
                    auto objectAsString = LocalRef<jstring> ((jstring) env->CallObjectMethod (object, JavaObject.toString));
                    auto objectClass    = LocalRef<jobject> (env->CallObjectMethod (object, JavaObject.getClass));
                    auto classAsString  = LocalRef<jstring> ((jstring) env->CallObjectMethod (objectClass, JavaClass.getName));

                    // Note: It seems that Firebase delivers values as strings always, so this check is rather unnecessary,
                    //       at least until they change the behaviour.
                    var value = juceString (classAsString) == "java.lang.Bundle" ? bundleToVar (object) : var (juceString (objectAsString.get()));
                    dynamicObject->setProperty (juceString (key.get()), value);
                }
                else
                {
                    dynamicObject->setProperty (juceString (key.get()), {});
                }
            }

            return var (dynamicObject.get());
        }

        return {};
    }

  #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
    static PushNotifications::Notification firebaseRemoteNotificationToJuceNotification (jobject remoteNotification)
    {
        auto* env = getEnv();

        auto collapseKey  = LocalRef<jstring> ((jstring) env->CallObjectMethod (remoteNotification, RemoteMessage.getCollapseKey));
        auto from         = LocalRef<jstring> ((jstring) env->CallObjectMethod (remoteNotification, RemoteMessage.getFrom));
        auto messageId    = LocalRef<jstring> ((jstring) env->CallObjectMethod (remoteNotification, RemoteMessage.getMessageId));
        auto messageType  = LocalRef<jstring> ((jstring) env->CallObjectMethod (remoteNotification, RemoteMessage.getMessageType));
        auto to           = LocalRef<jstring> ((jstring) env->CallObjectMethod (remoteNotification, RemoteMessage.getTo));
        auto notification = LocalRef<jobject> (env->CallObjectMethod (remoteNotification, RemoteMessage.getNotification));
        auto data         = LocalRef<jobject> (env->CallObjectMethod (remoteNotification, RemoteMessage.getData));

        const int64 sentTime = env->CallLongMethod (remoteNotification, RemoteMessage.getSentTime);
        const int ttl        = env->CallIntMethod  (remoteNotification, RemoteMessage.getTtl);

        auto keySet   = LocalRef<jobject> (env->CallObjectMethod (data, JavaMap.keySet));
        auto iterator = LocalRef<jobject> (env->CallObjectMethod (keySet, JavaSet.iterator));

        DynamicObject::Ptr dataDynamicObject = new DynamicObject();

        for (;;)
        {
            if (! env->CallBooleanMethod (iterator, JavaIterator.hasNext))
                break;

            auto key   = LocalRef<jstring> ((jstring) env->CallObjectMethod (iterator, JavaIterator.next));
            auto value = LocalRef<jstring> ((jstring) env->CallObjectMethod (data, JavaMap.get, key.get()));

            dataDynamicObject->setProperty (juceString (key.get()), juceString (value.get()));
        }

        var dataVar (dataDynamicObject.get());

        DynamicObject::Ptr propertiesDynamicObject = new DynamicObject();
        propertiesDynamicObject->setProperty ("collapseKey", juceString (collapseKey.get()));
        propertiesDynamicObject->setProperty ("from", juceString (from.get()));
        propertiesDynamicObject->setProperty ("messageId", juceString (messageId.get()));
        propertiesDynamicObject->setProperty ("messageType", juceString (messageType.get()));
        propertiesDynamicObject->setProperty ("to", juceString (to.get()));
        propertiesDynamicObject->setProperty ("sentTime", sentTime);
        propertiesDynamicObject->setProperty ("ttl", ttl);
        propertiesDynamicObject->setProperty ("data", dataVar);

        PushNotifications::Notification n;

        if (notification != 0)
        {
            auto body                  = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getBody));
            auto bodyLocalizationKey   = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getBodyLocalizationKey));
            auto clickAction           = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getClickAction));
            auto color                 = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getColor));
            auto icon                  = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getIcon));
            auto sound                 = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getSound));
            auto tag                   = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getTag));
            auto title                 = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getTitle));
            auto titleLocalizationKey  = LocalRef<jstring> ((jstring) env->CallObjectMethod (notification, RemoteMessageNotification.getTitleLocalizationKey));
            auto link                  = LocalRef<jobject> (env->CallObjectMethod (notification, RemoteMessageNotification.getLink));

            auto bodyLocalizationArgs  = LocalRef<jobjectArray> ((jobjectArray) env->CallObjectMethod (notification, RemoteMessageNotification.getBodyLocalizationArgs));
            auto titleLocalizationArgs = LocalRef<jobjectArray> ((jobjectArray) env->CallObjectMethod (notification, RemoteMessageNotification.getTitleLocalizationArgs));

            n.identifier = juceString (tag.get());
            n.title      = juceString (title.get());
            n.body       = juceString (body.get());
            n.soundToPlay = URL (juceString (sound.get()));

            auto colourString = juceString (color.get()).substring (1);
            const uint8 r = (uint8) colourString.substring (0, 2).getIntValue();
            const uint8 g = (uint8) colourString.substring (2, 4).getIntValue();
            const uint8 b = (uint8) colourString.substring (4, 6).getIntValue();
            n.accentColour = Colour (r, g, b);

            // Note: Ignoring the icon, because Firebase passes it as a string.

            propertiesDynamicObject->setProperty ("clickAction",           juceString (clickAction.get()));
            propertiesDynamicObject->setProperty ("bodyLocalizationKey",   juceString (bodyLocalizationKey.get()));
            propertiesDynamicObject->setProperty ("titleLocalizationKey",  juceString (titleLocalizationKey.get()));
            propertiesDynamicObject->setProperty ("bodyLocalizationArgs",  javaStringArrayToJuce (bodyLocalizationArgs));
            propertiesDynamicObject->setProperty ("titleLocalizationArgs", javaStringArrayToJuce (titleLocalizationArgs));
            propertiesDynamicObject->setProperty ("link",                  link.get() != nullptr ? juceString ((jstring) env->CallObjectMethod (link, AndroidUri.toString)) : String());
        }

        n.properties = var (propertiesDynamicObject.get());

        return n;
    }
  #endif

    void setupChannels (const Array<ChannelGroup>& groups, const Array<Channel>& channels)
    {
        if (getAndroidSDKVersion() < 26)
            return;

        auto* env = getEnv();

        auto notificationManager = getNotificationManager();

        jassert (notificationManager.get() != nullptr);

        if (notificationManager.get() == nullptr)
            return;

        for (const auto& g : groups)
        {
            // Channel group identifier and name have to be set.
            jassert (g.identifier.isNotEmpty() && g.name.isNotEmpty());

            if (g.identifier.isNotEmpty() && g.name.isNotEmpty())
            {
                auto group = LocalRef<jobject> (env->NewObject (NotificationChannelGroup, NotificationChannelGroup.constructor,
                                                                javaString (g.identifier).get(), javaString (g.name).get()));
                env->CallVoidMethod (notificationManager, NotificationManagerApi26.createNotificationChannelGroup, group.get());
            }
        }

        for (const auto& c : channels)
        {
            // Channel identifier, name and group have to be set.
            jassert (c.identifier.isNotEmpty() && c.name.isNotEmpty() && c.groupId.isNotEmpty());

            if (c.identifier.isEmpty() || c.name.isEmpty() || c.groupId.isEmpty())
                continue;

            auto channel = LocalRef<jobject> (env->NewObject (NotificationChannel, NotificationChannel.constructor,
                                                              javaString (c.identifier).get(), javaString (c.name).get(), c.importance));

            env->CallVoidMethod (channel, NotificationChannel.enableLights,            c.enableLights);
            env->CallVoidMethod (channel, NotificationChannel.enableVibration,         c.enableVibration);
            env->CallVoidMethod (channel, NotificationChannel.setBypassDnd,            c.bypassDoNotDisturb);
            env->CallVoidMethod (channel, NotificationChannel.setDescription,          javaString (c.description).get());
            env->CallVoidMethod (channel, NotificationChannel.setGroup,                javaString (c.groupId).get());
            env->CallVoidMethod (channel, NotificationChannel.setImportance,           c.importance);
            env->CallVoidMethod (channel, NotificationChannel.setLightColor,           c.ledColour.getARGB());
            env->CallVoidMethod (channel, NotificationChannel.setLockscreenVisibility, c.lockScreenAppearance);
            env->CallVoidMethod (channel, NotificationChannel.setShowBadge,            c.canShowBadge);


            const int size = c.vibrationPattern.size();

            if (size > 0)
            {
                auto array = LocalRef<jlongArray> (env->NewLongArray (size));
                jlong* elements = env->GetLongArrayElements (array, nullptr);

                for (int i = 0; i < size; ++i)
                    elements[i] = (jlong) c.vibrationPattern[i];

                env->SetLongArrayRegion (array, 0, size, elements);
                env->CallVoidMethod (channel, NotificationChannel.setVibrationPattern, array.get());

                env->CallVoidMethod (channel, NotificationChannel.enableVibration, c.enableVibration);
            }

            LocalRef<jobject> builder (env->NewObject (AndroidAudioAttributesBuilder, AndroidAudioAttributesBuilder.constructor));
            const int contentTypeSonification = 4;
            const int usageNotification = 5;
            env->CallObjectMethod (builder.get(), AndroidAudioAttributesBuilder.setContentType, contentTypeSonification);
            env->CallObjectMethod (builder.get(), AndroidAudioAttributesBuilder.setUsage, usageNotification);
            auto audioAttributes = LocalRef<jobject> (env->CallObjectMethod (builder.get(), AndroidAudioAttributesBuilder.build));
            env->CallVoidMethod (channel, NotificationChannel.setSound, juceUrlToAndroidUri (c.soundToPlay).get(), audioAttributes.get());

            env->CallVoidMethod (notificationManager, NotificationManagerApi26.createNotificationChannel, channel.get());
        }
    }

    void getPendingLocalNotifications() const {}
    void removePendingLocalNotification (const String&) {}
    void removeAllPendingLocalNotifications() {}

    static bool intentActionContainsAnyOf (jobject intent, const StringArray& strings, bool includePackageName)
    {
        auto* env = getEnv();
        LocalRef<jobject> context (getMainActivity());

        String packageName = includePackageName ? juceString ((jstring) env->CallObjectMethod (context.get(),
                                                                                               AndroidContext.getPackageName))
                                                : String{};

        String intentAction = juceString ((jstring) env->CallObjectMethod (intent, AndroidIntent.getAction));

        for (const auto& string : strings)
            if (intentAction.contains (packageName + string))
                return true;

        return false;
    }

    static bool isDeleteNotificationIntent (jobject intent)
    {
        return intentActionContainsAnyOf (intent, StringArray (".JUCE_NOTIFICATION_DELETED"), true);
    }

    static bool isLocalNotificationIntent (jobject intent)
    {
        return intentActionContainsAnyOf (intent, { ".JUCE_NOTIFICATION.",
                                                    ".JUCE_NOTIFICATION_BUTTON_ACTION.",
                                                    ".JUCE_NOTIFICATION_TEXT_INPUT_ACTION." },
                                          true);
    }

    static bool isRemoteNotificationIntent (jobject intent)
    {
        auto* env = getEnv();

        auto categories = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getCategories));

        int categoriesNum = categories != nullptr
                          ? env->CallIntMethod (categories, JavaSet.size)
                          : 0;

        if (categoriesNum == 0)
            return false;

        if (! env->CallBooleanMethod (categories, JavaSet.contains, javaString ("android.intent.category.LAUNCHER").get()))
            return false;

        if (! intentActionContainsAnyOf (intent, StringArray ("android.intent.action.MAIN"), false))
            return false;

        auto extras = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getExtras));

        if (extras == nullptr)
            return false;

        return env->CallBooleanMethod (extras, AndroidBundle.containsKey, javaString ("google.sent_time").get())
            && env->CallBooleanMethod (extras, AndroidBundle.containsKey, javaString ("google.message_id").get());
    }

    static bool isBrowsableNotificationIntent (jobject intent)
    {
        auto* env = getEnv();

        auto intentAction = juceString ((jstring) env->CallObjectMethod (intent, AndroidIntent.getAction));

        auto isView = intentAction == "android.intent.action.VIEW";

        auto categories = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getCategories));

        int categoriesNum = categories != nullptr
                            ? env->CallIntMethod (categories, JavaSet.size)
                            : 0;

        if (categoriesNum == 0)
            return false;

        if (! env->CallBooleanMethod (categories, JavaSet.contains, javaString ("android.intent.category.BROWSABLE").get()))
            return false;

        return isView;
    }

    static void callAnotherInstanceStartedWithIntentURI (jobject intent)
    {
        auto* env = getEnv();

        auto intentDataUri = LocalRef<jobject> (env->CallObjectMethod (intent, AndroidIntent.getData));
        auto intentDataString = juceString ((jstring) env->CallObjectMethod (intentDataUri, AndroidUri.toString));

        JUCEApplication::getInstance()->anotherInstanceStarted (intentDataString);
    }

    PushNotifications& owner;
};

#if defined(JUCE_FIREBASE_INSTANCE_ID_SERVICE_CLASSNAME)
//==============================================================================
struct JuceFirebaseInstanceIdService
{
    #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
     CALLBACK (tokenRefreshed, "firebaseInstanceIdTokenRefreshed", "(Ljava/lang/String;)V")

     DECLARE_JNI_CLASS (InstanceIdService, "com/rmsl/juce/JuceFirebaseInstanceIdService")
    #undef JNI_CLASS_MEMBERS

    static void JNICALL tokenRefreshed (JNIEnv*, jobject /*instanceIdService*/, void* token)
    {
        if (auto* instance = PushNotifications::getInstanceWithoutCreating())
            instance->pimpl->notifyListenersTokenRefreshed (juceString (static_cast<jstring> (token)));
    }
};

JuceFirebaseInstanceIdService::InstanceIdService_Class JuceFirebaseInstanceIdService::InstanceIdService;
#endif

#if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
//==============================================================================
struct JuceFirebaseMessagingService
{
    #define JNI_CLASS_MEMBERS(METHOD, STATICMETHOD, FIELD, STATICFIELD, CALLBACK) \
     CALLBACK (remoteNotificationReceived,  "firebaseRemoteMessageReceived",  "(Lcom/google/firebase/messaging/RemoteMessage;)V") \
     CALLBACK (remoteMessagesDeleted,       "firebaseRemoteMessagesDeleted",  "()V") \
     CALLBACK (remoteMessageSent,           "firebaseRemoteMessageSent",      "(Ljava/lang/String;)V") \
     CALLBACK (remoteMessageSendError,      "firebaseRemoteMessageSendError", "(Ljava/lang/String;Ljava/lang/String;)V")

     DECLARE_JNI_CLASS (MessagingService, "com/rmsl/juce/JuceFirebaseMessagingService")
    #undef JNI_CLASS_MEMBERS

    static void JNICALL remoteNotificationReceived (JNIEnv*, jobject /*messagingService*/, void* remoteMessage)
    {
        if (auto* instance = PushNotifications::getInstanceWithoutCreating())
            instance->pimpl->notifyListenersAboutRemoteNotificationFromService (LocalRef<jobject> (static_cast<jobject> (remoteMessage)));

    }

    static void JNICALL remoteMessagesDeleted()
    {
        if (auto* instance = PushNotifications::getInstanceWithoutCreating())
            instance->pimpl->notifyListenersAboutRemoteNotificationsDeleted();
    }

    static void JNICALL remoteMessageSent (JNIEnv*, jobject /*messagingService*/, void* messageId)
    {
        if (auto* instance = PushNotifications::getInstanceWithoutCreating())
            instance->pimpl->notifyListenersAboutUpstreamMessageSent (LocalRef<jstring> (static_cast<jstring> (messageId)));
    }

    static void JNICALL remoteMessageSendError (JNIEnv*, jobject /*messagingService*/, void* messageId, void* error)
    {
        if (auto* instance = PushNotifications::getInstanceWithoutCreating())
            instance->pimpl->notifyListenersAboutUpstreamMessageSendingError (LocalRef<jstring> (static_cast<jstring> (messageId)),
                                                                              LocalRef<jstring> (static_cast<jstring> (error)));
    }
};

JuceFirebaseMessagingService::MessagingService_Class  JuceFirebaseMessagingService::MessagingService;
#endif

//==============================================================================
bool juce_handleNotificationIntent (void* intent)
{
    auto* instance = PushNotifications::getInstanceWithoutCreating();

    if (PushNotifications::Pimpl::isDeleteNotificationIntent ((jobject) intent))
    {
        if (instance)
            instance->pimpl->notifyListenersAboutLocalNotificationDeleted (LocalRef<jobject> ((jobject) intent));

        return true;
    }
    else if (PushNotifications::Pimpl::isLocalNotificationIntent ((jobject) intent))
    {
        if (instance)
            instance->pimpl->notifyListenersAboutLocalNotification (LocalRef<jobject> ((jobject) intent));

        return true;
    }
    else if (PushNotifications::Pimpl::isBrowsableNotificationIntent ((jobject) intent))
    {
        return true;
    }
  #if defined(JUCE_FIREBASE_MESSAGING_SERVICE_CLASSNAME)
    else if (PushNotifications::Pimpl::isRemoteNotificationIntent ((jobject) intent))
    {
        if (instance)
            instance->pimpl->notifyListenersAboutRemoteNotificationFromSystemTray (LocalRef<jobject> ((jobject) intent));

        return true;
    }
  #endif

    return false;
}

} // namespace juce
