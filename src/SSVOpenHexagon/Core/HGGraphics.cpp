// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#include "SSVOpenHexagon/Core/HexagonGame.hpp"

#include "SSVOpenHexagon/Components/CWall.hpp"

#include "SSVOpenHexagon/Global/Assert.hpp"
#include "SSVOpenHexagon/Global/Assets.hpp"
#include "SSVOpenHexagon/Global/Config.hpp"
#include "SSVOpenHexagon/Global/Imgui.hpp"

#include "SSVOpenHexagon/Utils/Color.hpp"
#include "SSVOpenHexagon/Utils/Math.hpp"
#include "SSVOpenHexagon/Utils/String.hpp"

#include "SSVStart/Utils/SFML.hpp"

#include <SFML/Graphics/RenderStates.hpp>
#include <SSVStart/Utils/SFML.hpp>

#include <SSVUtils/Core/Log/Log.hpp>
#include <SSVUtils/Core/Utils/Rnd.hpp>

#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/RenderTexture.hpp>

#include <cstdint>

namespace hg {

[[nodiscard]] static std::string formatTime(const double x)
{
    return ssvu::toStr(std::floor(x * 1000) / 1000.f);
}

template <typename... Ts>
void HexagonGame::render(Ts&&... xs)
{
    if (window == nullptr)
    {
        ssvu::lo("hg::HexagonGame::render")
            << "Attempted to render without a game window\n";

        return;
    }

    window->draw(SSVOH_FWD(xs)...);
}

void HexagonGame::draw()
{
    if (window == nullptr || Config::getDisableGameRendering())
    {
        return;
    }

    const auto getRenderStates = [this](
                                     const RenderStage rs) -> sf::RenderStates
    {
        if (!Config::getShaders())
        {
            return sf::RenderStates::Default;
        }

        const sf::base::Optional<std::size_t> fragmentShaderId =
            status.fragmentShaderIds[static_cast<std::size_t>(rs)];

        if (!fragmentShaderId.hasValue())
        {
            return sf::RenderStates::Default;
        }

        runLuaFunctionIfExists<int, float>(
            "onRenderStage", static_cast<int>(rs), 60.f / window->getFPS());
        return sf::RenderStates{assets.getShaderByShaderId(*fragmentShaderId)};
    };

    SSVOH_ASSERT(backgroundCamera.hasValue());
    SSVOH_ASSERT(overlayCamera.hasValue());

    window->clear(sf::Color::Black);

    if (!status.hasDied)
    {
        if (levelStatus.cameraShake > 0.f)
        {
            const sf::Vector2f shake(ssvu::getRndR(-levelStatus.cameraShake,
                                         levelStatus.cameraShake),
                ssvu::getRndR(
                    -levelStatus.cameraShake, levelStatus.cameraShake));

            backgroundCamera->setCenter(shake);
            overlayCamera->setCenter(
                shake + sf::Vector2f{Config::getWidth() / 2.f,
                            Config::getHeight() / 2.f});
        }
        else
        {
            backgroundCamera->setCenter(sf::Vector2f::Zero);
            overlayCamera->setCenter(sf::Vector2f{
                Config::getWidth() / 2.f, Config::getHeight() / 2.f});
        }
    }

    if (!Config::getNoBackground())
    {
        window->setView(backgroundCamera->apply());

        backgroundTris.clear();

        styleData.drawBackground(backgroundTris, sf::Vector2f::Zero,
            levelStatus.sides,
            Config::getDarkenUnevenBackgroundChunk() &&
                levelStatus.darkenUnevenBackgroundChunk,
            Config::getBlackAndWhite());

        render(backgroundTris, getRenderStates(RenderStage::BackgroundTris));
    }

    window->setView(backgroundCamera->apply());

    wallQuads3D.clear();
    pivotQuads3D.clear();
    playerTris3D.clear();
    wallQuads.clear();
    pivotQuads.clear();
    playerTris.clear();
    capTris.clear();

    // Reserve right amount of memory for all walls and custom walls
    wallQuads.reserve_more_quad(walls.size() + cwManager.count());

    for (CWall& w : walls)
    {
        w.draw(getColorWall(), wallQuads);
    }

    cwManager.draw(wallQuads);

    if (status.started)
    {
        player.draw(getSides(), getColorMain(), getColorPlayer(), pivotQuads,
            capTris, playerTris, getColorCap(), Config::getAngleTiltIntensity(),
            Config::getShowSwapBlinkingEffect());
    }

    if (Config::get3D())
    {
        const float depth(styleData._3dDepth);
        const std::size_t numWallQuads(wallQuads.size());
        const std::size_t numPivotQuads(pivotQuads.size());
        const std::size_t numPlayerTris(playerTris.size());

        wallQuads3D.reserve(numWallQuads * depth);
        pivotQuads3D.reserve(numPivotQuads * depth);
        playerTris3D.reserve(numPlayerTris * depth);

        const float pulse3D{Config::getNoPulse() ? 1.f : status.pulse3D};
        const float effect{
            styleData._3dSkew * Config::get3DMultiplier() * pulse3D};

        const sf::Vector2f skew{1.f, 1.f + effect};
        backgroundCamera->setSkew(skew);

        const float radRot(
            Utils::toRad(backgroundCamera->getRotation()) + (Utils::pi / 2.f));
        const float sinRot(std::sin(radRot));
        const float cosRot(std::cos(radRot));

        for (std::size_t i = 0; i < depth; ++i)
        {
            wallQuads3D.unsafe_emplace_other(wallQuads);
            pivotQuads3D.unsafe_emplace_other(pivotQuads);
            playerTris3D.unsafe_emplace_other(playerTris);
        }

        const auto adjustAlpha = [&](sf::Color& c, const float i)
        {
            if (styleData._3dAlphaMult == 0.f)
            {
                return;
            }

            const float newAlpha =
                (static_cast<float>(c.a) / styleData._3dAlphaMult) -
                i * styleData._3dAlphaFalloff;

            c.a = Utils::componentClamp(newAlpha);
        };

        for (int j(0); j < static_cast<int>(depth); ++j)
        {
            const float i(depth - j - 1);

            const float offset(styleData._3dSpacing *
                               (float(i + 1.f) * styleData._3dPerspectiveMult) *
                               (effect * 3.6f) * 1.4f);

            const sf::Vector2f newPos(offset * cosRot, offset * sinRot);

            sf::Color overrideColor;

            if (!Config::getBlackAndWhite())
            {
                overrideColor = Utils::getColorDarkened(
                    styleData.get3DOverrideColor(), styleData._3dDarkenMult);
            }
            else
            {
                overrideColor = Utils::getColorDarkened(
                    sf::Color(255, 255, 255, styleData.getMainColor().a),
                    styleData._3dDarkenMult);
            }
            adjustAlpha(overrideColor, i);

            // Draw pivot layers
            for (std::size_t k = j * numPivotQuads; k < (j + 1) * numPivotQuads;
                 ++k)
            {
                pivotQuads3D[k].position += newPos;
                pivotQuads3D[k].color = overrideColor;
            }

            if (styleData.get3DOverrideColor() == styleData.getMainColor())
            {
                overrideColor = Utils::getColorDarkened(
                    getColorWall(), styleData._3dDarkenMult);

                adjustAlpha(overrideColor, i);
            }

            // Draw wall layers
            for (std::size_t k = j * numWallQuads; k < (j + 1) * numWallQuads;
                 ++k)
            {
                wallQuads3D[k].position += newPos;
                wallQuads3D[k].color = overrideColor;
            }

            // Apply player color if no 3D override is present.
            if (styleData.get3DOverrideColor() == styleData.getMainColor())
            {
                overrideColor = Utils::getColorDarkened(
                    getColorPlayer(), styleData._3dDarkenMult);

                adjustAlpha(overrideColor, i);
            }

            // Draw player layers
            for (std::size_t k = j * numPlayerTris; k < (j + 1) * numPlayerTris;
                 ++k)
            {
                playerTris3D[k].position += newPos;
                playerTris3D[k].color = overrideColor;
            }
        }
    }

    render(wallQuads3D, getRenderStates(RenderStage::WallQuads3D));
    render(pivotQuads3D, getRenderStates(RenderStage::PivotQuads3D));
    render(playerTris3D, getRenderStates(RenderStage::PlayerTris3D));

    if (Config::getShowPlayerTrail() && status.showPlayerTrail)
    {
        drawTrailParticles();
    }

    if (Config::getShowSwapParticles())
    {
        drawSwapParticles();
    }

    render(wallQuads, getRenderStates(RenderStage::WallQuads));
    render(capTris, getRenderStates(RenderStage::CapTris));
    render(pivotQuads, getRenderStates(RenderStage::PivotQuads));
    render(playerTris, getRenderStates(RenderStage::PlayerTris));

    window->setView(overlayCamera->apply());

    drawParticles();
    drawText(getRenderStates(RenderStage::Text));

    // ------------------------------------------------------------------------
    // Draw key icons.
    if (Config::getShowKeyIcons() || mustShowReplayUI())
    {
        drawKeyIcons();
    }

    // ------------------------------------------------------------------------
    // Draw level info.
    if (Config::getShowLevelInfo() || mustShowReplayUI())
    {
        drawLevelInfo(getRenderStates(RenderStage::Text));
    }

    // ------------------------------------------------------------------------
    if (Config::getFlash())
    {
        render(flashPolygon);
    }

    if (mustTakeScreenshot)
    {
        if (window != nullptr)
        {
            SSVOH_ASSERT(graphicsContext != nullptr);
            window->saveScreenshot(*graphicsContext, "screenshot.png");
        }

        mustTakeScreenshot = false;
    }

    drawImguiLuaConsole();
}

void HexagonGame::drawImguiLuaConsole()
{
    if (window == nullptr)
    {
        return;
    }

    if (!ilcShowConsole)
    {
        return;
    }

    SSVOH_ASSERT(overlayCamera.hasValue());

    sf::RenderWindow& renderWindow = window->getRenderWindow();
    window->setView(renderWindow.getDefaultView());

    Imgui::render(*window);
}

void HexagonGame::initFlashEffect(int r, int g, int b)
{
    flashPolygon.clear();
    flashPolygon.reserve(6);

    const sf::Color color{static_cast<std::uint8_t>(r),
        static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), 0};

    const auto width = static_cast<float>(Config::getWidth());
    const auto height = static_cast<float>(Config::getHeight());
    const float offset = 100.f;

    const sf::Vector2f nw{-offset, -offset};
    const sf::Vector2f sw{-offset, height + offset};
    const sf::Vector2f se{width + offset, height + offset};
    const sf::Vector2f ne{width + offset, -offset};

    flashPolygon.batch_unsafe_emplace_back_quad(color, nw, sw, se, ne);
}

void HexagonGame::drawKeyIcons()
{
    constexpr std::uint8_t offOpacity = 90;
    constexpr std::uint8_t onOpacity = 255;

    const sf::Color colorText = getColorText();

    const sf::Color offColor{colorText.r, colorText.g, colorText.b, offOpacity};
    const sf::Color onColor{colorText.r, colorText.g, colorText.b, onOpacity};

    keyIconLeft.setColor((getInputMovement() == -1) ? onColor : offColor);
    keyIconRight.setColor((getInputMovement() == 1) ? onColor : offColor);
    keyIconFocus.setColor(getInputFocused() ? onColor : offColor);
    keyIconSwap.setColor(getInputSwap() ? onColor : offColor);

    render(keyIconLeft, *txKeyIconLeft);
    render(keyIconRight, *txKeyIconRight);
    render(keyIconFocus, *txKeyIconFocus);
    render(keyIconSwap, *txKeyIconSwap);

    // ------------------------------------------------------------------------

    if (mustShowReplayUI())
    {
        replayIcon.setColor(onColor);
        render(replayIcon, *txReplayIcon);
    }
}

void HexagonGame::drawLevelInfo(const sf::RenderStates& mStates)
{
    render(levelInfoRectangle, /* texture */ nullptr, mStates);

    if (textUI.hasValue())
    {
        render(textUI->levelInfoTextLevel, mStates);
        render(textUI->levelInfoTextPack, mStates);
        render(textUI->levelInfoTextAuthor, mStates);
        render(textUI->levelInfoTextBy, mStates);
        render(textUI->levelInfoTextDM, mStates);
    }
}

void HexagonGame::drawParticles()
{
    for (Particle& p : particles)
    {
        render(p.sprite, *txStarParticle);
    }
}

void HexagonGame::drawTrailParticles()
{
    for (TrailParticle& p : trailParticles)
    {
        render(p.sprite, *txSmallCircle);
    }
}

void HexagonGame::drawSwapParticles()
{
    for (SwapParticle& p : swapParticles)
    {
        render(p.sprite, *txSmallCircle);
    }
}

void HexagonGame::updateText(float mFT)
{
    if (window == nullptr || !textUI.hasValue())
    {
        return;
    }

    // ------------------------------------------------------------------------
    // Update "personal best" text animation.
    pbTextGrowth += 0.08f * mFT;
    if (pbTextGrowth > Utils::pi * 2.f)
    {
        pbTextGrowth = 0;
    }

    // ------------------------------------------------------------------------
    os.str("");

    if (debugPause)
    {
        os << "(!) PAUSED (!)\n";
    }

    if (levelStatus.tutorialMode)
    {
        os << "TUTORIAL MODE\n";
    }
    else if (Config::getOfficial())
    {
        os << "OFFICIAL MODE\n";
    }

    if (Config::getDebug())
    {
        os << "DEBUG MODE\n";

        os << "CUSTOM WALLS: " << cwManager.count() << " / "
           << cwManager.maxHandles() << '\n';
    }

    if (status.started)
    {
        if (levelStatus.swapEnabled)
        {
            os << "SWAP ENABLED\n";
        }

        if (Config::getInvincible())
        {
            os << "INVINCIBILITY ON\n";
        }

        if (const float timescale = Config::getTimescale(); timescale != 1.f)
        {
            os << "TIMESCALE " << timescale << '\n';
        }

        if (status.scoreInvalid)
        {
            os << "SCORE INVALIDATED (" << status.invalidReason << ")\n";
        }

        if (status.hasDied)
        {
            os << status.restartInput;
            os << status.replayInput;
        }

        if (calledDeprecatedFunctions.size() > 1)
        {
            os << calledDeprecatedFunctions.size()
               << " WARNINGS RAISED (CHECK CONSOLE)\n";
        }
        else if (calledDeprecatedFunctions.size() > 0)
        {
            os << "1 WARNING RAISED (CHECK CONSOLE)\n";
        }

        const auto& trackedVariables(levelStatus.trackedVariables);
        if (Config::getShowTrackedVariables() && !trackedVariables.empty())
        {
            os << '\n';
            for (const auto& [variableName, display] : trackedVariables)
            {
                if (!lua.doesVariableExist(variableName))
                {
                    continue;
                }

                const std::string value{
                    lua.readVariable<std::string>(variableName)};

                os << Utils::toUppercase(display) << ": "
                   << Utils::toUppercase(value) << '\n';
            }
        }
    }
    else if (Config::getRotateToStart())
    {
        os << "ROTATE TO START\n";
        textUI->messageText.setString("ROTATE TO START");
    }

    os.flush();

    // Set in game timer text
    if (!levelStatus.scoreOverridden)
    {
        // By default, use the timer for scoring
        if (status.started)
        {
            textUI->timeText.setString(formatTime(status.getTimeSeconds()));
        }
        else
        {
            textUI->timeText.setString("0");
        }
    }
    else
    {
        // Alternative scoring
        textUI->timeText.setString(
            lua.readVariable<std::string>(levelStatus.scoreOverride));
    }

    const auto getScaledCharacterSize = [&](const float size)
    {
        return ssvu::toNum<unsigned int>(
            size / Config::getZoomFactor() * Config::getTextScaling());
    };

    textUI->timeText.setCharacterSize(getScaledCharacterSize(70.f));

    // Set information text
    textUI->text.setString(os.str());
    textUI->text.setCharacterSize(getScaledCharacterSize(20.f));
    textUI->text.setOrigin({0.f, 0.f});

    // Set FPS Text, if option is enabled.
    if (Config::getShowFPS())
    {
        textUI->fpsText.setString(ssvu::toStr(window->getFPS()));
        textUI->fpsText.setCharacterSize(getScaledCharacterSize(20.f));
    }

    textUI->messageText.setCharacterSize(getScaledCharacterSize(32.f));
    textUI->messageText.setOrigin(
        {ssvs::getGlobalWidth(textUI->messageText) / 2.f, 0.f});

    const float growth = std::sin(pbTextGrowth);
    textUI->pbText.setCharacterSize(
        getScaledCharacterSize(64.f) + growth * 10.f);
    textUI->pbText.setOrigin({ssvs::getGlobalWidth(textUI->pbText) / 2.f, 0.f});

    // ------------------------------------------------------------------------

    if (mustShowReplayUI())
    {
        const replay_file& rf = activeReplay->replayFile;

        os.str("");

        if (!levelStatus.scoreOverridden)
        {
            os << formatTime(rf.played_seconds()) << "s";
        }
        else
        {
            os << formatTime(rf._played_score);
        }

        os << " BY " << rf._player_name;

        os.flush();

        textUI->replayText.setCharacterSize(getScaledCharacterSize(16.f));
        textUI->replayText.setString(os.str());
    }
    else
    {
        textUI->replayText.setString("");
    }
}

void HexagonGame::drawText_TimeAndStatus(
    const sf::Color& offsetColor, const sf::RenderStates& mStates)
{
    if (!textUI.hasValue())
    {
        return;
    }

    if (Config::getDrawTextOutlines())
    {
        textUI->timeText.setOutlineColor(offsetColor);
        textUI->text.setOutlineColor(offsetColor);
        textUI->fpsText.setOutlineColor(offsetColor);
        textUI->replayText.setOutlineColor(offsetColor);

        textUI->timeText.setOutlineThickness(2.f);
        textUI->text.setOutlineThickness(1.f);
        textUI->fpsText.setOutlineThickness(1.f);
        textUI->replayText.setOutlineThickness(1.f);
    }
    else
    {
        textUI->timeText.setOutlineThickness(0.f);
        textUI->text.setOutlineThickness(0.f);
        textUI->fpsText.setOutlineThickness(0.f);
        textUI->replayText.setOutlineThickness(0.f);
    }

    const float padding =
        (Config::getTextPadding() * Config::getTextScaling()) /
        Config::getZoomFactor();

    const sf::Color colorText = getColorText();

    if (Config::getShowTimer())
    {
        textUI->timeText.setFillColor(colorText);
        textUI->timeText.setOrigin(ssvs::getLocalNW(textUI->timeText));
        textUI->timeText.setPosition({padding, padding});

        render(textUI->timeText, mStates);
    }

    if (Config::getShowStatusText())
    {
        textUI->text.setFillColor(colorText);
        textUI->text.setOrigin(ssvs::getLocalNW(textUI->text));
        textUI->text.setPosition(
            {padding, ssvs::getGlobalBottom(textUI->timeText) + padding});

        render(textUI->text, mStates);
    }

    if (Config::getShowFPS())
    {
        textUI->fpsText.setFillColor(colorText);
        textUI->fpsText.setOrigin(ssvs::getLocalSW(textUI->fpsText));

        if (Config::getShowLevelInfo() || mustShowReplayUI())
        {
            textUI->fpsText.setPosition(
                {padding, ssvs::getGlobalTop(levelInfoRectangle) - padding});
        }
        else
        {
            textUI->fpsText.setPosition(
                {padding, Config::getHeight() - padding});
        }

        render(textUI->fpsText, mStates);
    }

    if (mustShowReplayUI())
    {
        const float scaling =
            Config::getKeyIconsScale() / Config::getZoomFactor();

        const float replayPadding = 8.f * scaling;

        textUI->replayText.setFillColor(colorText);
        textUI->replayText.setOrigin(ssvs::getLocalCenterE(textUI->replayText));
        textUI->replayText.setPosition(ssvs::getGlobalCenterW(replayIcon) -
                                       sf::Vector2f{replayPadding, 0});
        render(textUI->replayText, mStates);
    }
}

template <typename FRender>
static void drawTextMessagePBImpl(sf::Text& text, const sf::Color& offsetColor,
    const sf::Vector2f& pos, const sf::Color& color, float outlineThickness,
    FRender&& fRender)
{
    if (text.getString().isEmpty())
    {
        return;
    }

    if (Config::getDrawTextOutlines())
    {
        text.setOutlineColor(offsetColor);
        text.setOutlineThickness(outlineThickness);
    }
    else
    {
        text.setOutlineThickness(0.f);
    }

    text.setPosition(pos);
    text.setFillColor(color);

    fRender(text);
}

void HexagonGame::drawText_Message(
    const sf::Color& offsetColor, const sf::RenderStates& mStates)
{
    drawTextMessagePBImpl(textUI->messageText, offsetColor,
        {Config::getWidth() / 2.f, Config::getHeight() / 5.5f}, getColorText(),
        1.f /* outlineThickness */,
        [this, &mStates](sf::Text& t) { render(t, mStates); });
}

void HexagonGame::drawText_PersonalBest(
    const sf::Color& offsetColor, const sf::RenderStates& mStates)
{
    drawTextMessagePBImpl(textUI->pbText, offsetColor,
        {Config::getWidth() / 2.f,
            Config::getHeight() - Config::getHeight() / 4.f},
        getColorText(), 4.f /* outlineThickness */,
        [this, &mStates](sf::Text& t) { render(t, mStates); });
}

void HexagonGame::drawText(const sf::RenderStates& mStates)
{
    const sf::Color offsetColor{
        Config::getBlackAndWhite() || styleData.getColors().empty()
            ? sf::Color::Black
            : getColor(0)};

    drawText_TimeAndStatus(offsetColor, mStates);
    drawText_Message(offsetColor, mStates);
    drawText_PersonalBest(offsetColor, mStates);
}

} // namespace hg
