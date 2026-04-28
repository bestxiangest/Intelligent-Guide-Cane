from flask import Flask

from routes.ai_routes import ai_bp
from routes.gps_routes import gps_bp
from routes.health_routes import health_bp
from routes.navigation_routes import navigation_bp
from routes.weather_routes import weather_bp


def create_app(test_config=None):
    app = Flask(__name__)
    app.config["JSON_AS_ASCII"] = False

    if test_config:
        app.config.update(test_config)

    app.register_blueprint(health_bp)
    app.register_blueprint(ai_bp)
    app.register_blueprint(gps_bp)
    app.register_blueprint(weather_bp)
    app.register_blueprint(navigation_bp)
    return app


app = create_app()


if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=12345)
