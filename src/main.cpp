/**
 * @file     : main.cpp
 * @brief    : Application entry point and Qt Quick setup.
 * @details  : Starts the AeroTwin Ground Control Station user interface.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSurfaceFormat>

/**
 * @brief Configures and runs the AeroTwin Qt Quick application.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument values.
 * @return Application event-loop exit status.
 */
int main(int argc, char *argv[])
{
    // Set multisampling before creating the application or any render surfaces.
    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("AeroTwin"));
    app.setApplicationName(QStringLiteral("AeroTwin GCS"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    // Exit instead of entering an unusable event loop if the root QML object fails.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("AeroTwinGCS", "Main");

    return app.exec();
}
