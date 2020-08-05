/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// TODO(b/129481165): remove the #pragma below and fix conversion issues
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

#include "LayerTransactionTest.h"

namespace android {

class ScreenCaptureTest : public LayerTransactionTest {
protected:
    virtual void SetUp() {
        LayerTransactionTest::SetUp();
        ASSERT_EQ(NO_ERROR, mClient->initCheck());

        const auto display = SurfaceComposerClient::getInternalDisplayToken();
        ASSERT_FALSE(display == nullptr);

        DisplayConfig config;
        ASSERT_EQ(NO_ERROR, SurfaceComposerClient::getActiveDisplayConfig(display, &config));
        const ui::Size& resolution = config.resolution;

        // Background surface
        mBGSurfaceControl = createLayer(String8("BG Test Surface"), resolution.getWidth(),
                                        resolution.getHeight(), 0);
        ASSERT_TRUE(mBGSurfaceControl != nullptr);
        ASSERT_TRUE(mBGSurfaceControl->isValid());
        TransactionUtils::fillSurfaceRGBA8(mBGSurfaceControl, 63, 63, 195);

        // Foreground surface
        mFGSurfaceControl = createLayer(String8("FG Test Surface"), 64, 64, 0);

        ASSERT_TRUE(mFGSurfaceControl != nullptr);
        ASSERT_TRUE(mFGSurfaceControl->isValid());

        TransactionUtils::fillSurfaceRGBA8(mFGSurfaceControl, 195, 63, 63);

        asTransaction([&](Transaction& t) {
            t.setDisplayLayerStack(display, 0);

            t.setLayer(mBGSurfaceControl, INT32_MAX - 2).show(mBGSurfaceControl);

            t.setLayer(mFGSurfaceControl, INT32_MAX - 1)
                    .setPosition(mFGSurfaceControl, 64, 64)
                    .show(mFGSurfaceControl);
        });
    }

    virtual void TearDown() {
        LayerTransactionTest::TearDown();
        mBGSurfaceControl = 0;
        mFGSurfaceControl = 0;
    }

    sp<SurfaceControl> mBGSurfaceControl;
    sp<SurfaceControl> mFGSurfaceControl;
    std::unique_ptr<ScreenCapture> mCapture;
};

TEST_F(ScreenCaptureTest, CaptureSingleLayer) {
    auto bgHandle = mBGSurfaceControl->getHandle();
    ScreenCapture::captureLayers(&mCapture, bgHandle);
    mCapture->expectBGColor(0, 0);
    // Doesn't capture FG layer which is at 64, 64
    mCapture->expectBGColor(64, 64);
}

TEST_F(ScreenCaptureTest, CaptureLayerWithChild) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);

    SurfaceComposerClient::Transaction().show(child).apply(true);

    // Captures mFGSurfaceControl layer and its child.
    ScreenCapture::captureLayers(&mCapture, fgHandle);
    mCapture->expectFGColor(10, 10);
    mCapture->expectChildColor(0, 0);
}

TEST_F(ScreenCaptureTest, CaptureLayerChildOnly) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);

    SurfaceComposerClient::Transaction().show(child).apply(true);

    // Captures mFGSurfaceControl's child
    ScreenCapture::captureChildLayers(&mCapture, fgHandle);
    mCapture->checkPixel(10, 10, 0, 0, 0);
    mCapture->expectChildColor(0, 0);
}

TEST_F(ScreenCaptureTest, CaptureLayerExclude) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);
    sp<SurfaceControl> child2 = createSurface(mClient, "Child surface", 10, 10,
                                              PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child2, 200, 0, 200);

    SurfaceComposerClient::Transaction()
            .show(child)
            .show(child2)
            .setLayer(child, 1)
            .setLayer(child2, 2)
            .apply(true);

    // Child2 would be visible but its excluded, so we should see child1 color instead.
    ScreenCapture::captureChildLayersExcluding(&mCapture, fgHandle, {child2->getHandle()});
    mCapture->checkPixel(10, 10, 0, 0, 0);
    mCapture->checkPixel(0, 0, 200, 200, 200);
}

// Like the last test but verifies that children are also exclude.
TEST_F(ScreenCaptureTest, CaptureLayerExcludeTree) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);
    sp<SurfaceControl> child2 = createSurface(mClient, "Child surface", 10, 10,
                                              PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child2, 200, 0, 200);
    sp<SurfaceControl> child3 = createSurface(mClient, "Child surface", 10, 10,
                                              PIXEL_FORMAT_RGBA_8888, 0, child2.get());
    TransactionUtils::fillSurfaceRGBA8(child2, 200, 0, 200);

    SurfaceComposerClient::Transaction()
            .show(child)
            .show(child2)
            .show(child3)
            .setLayer(child, 1)
            .setLayer(child2, 2)
            .apply(true);

    // Child2 would be visible but its excluded, so we should see child1 color instead.
    ScreenCapture::captureChildLayersExcluding(&mCapture, fgHandle, {child2->getHandle()});
    mCapture->checkPixel(10, 10, 0, 0, 0);
    mCapture->checkPixel(0, 0, 200, 200, 200);
}

TEST_F(ScreenCaptureTest, CaptureTransparent) {
    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());

    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);

    SurfaceComposerClient::Transaction().show(child).apply(true);

    auto childHandle = child->getHandle();

    // Captures child
    ScreenCapture::captureLayers(&mCapture, childHandle, {0, 0, 10, 20});
    mCapture->expectColor(Rect(0, 0, 9, 9), {200, 200, 200, 255});
    // Area outside of child's bounds is transparent.
    mCapture->expectColor(Rect(0, 10, 9, 19), {0, 0, 0, 0});
}

TEST_F(ScreenCaptureTest, DontCaptureRelativeOutsideTree) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    ASSERT_NE(nullptr, child.get()) << "failed to create surface";
    sp<SurfaceControl> relative = createLayer(String8("Relative surface"), 10, 10, 0);
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);
    TransactionUtils::fillSurfaceRGBA8(relative, 100, 100, 100);

    SurfaceComposerClient::Transaction()
            .show(child)
            // Set relative layer above fg layer so should be shown above when computing all layers.
            .setRelativeLayer(relative, fgHandle, 1)
            .show(relative)
            .apply(true);

    // Captures mFGSurfaceControl layer and its child. Relative layer shouldn't be captured.
    ScreenCapture::captureLayers(&mCapture, fgHandle);
    mCapture->expectFGColor(10, 10);
    mCapture->expectChildColor(0, 0);
}

TEST_F(ScreenCaptureTest, CaptureRelativeInTree) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    sp<SurfaceControl> relative = createSurface(mClient, "Relative surface", 10, 10,
                                                PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);
    TransactionUtils::fillSurfaceRGBA8(relative, 100, 100, 100);

    SurfaceComposerClient::Transaction()
            .show(child)
            // Set relative layer below fg layer but relative to child layer so it should be shown
            // above child layer.
            .setLayer(relative, -1)
            .setRelativeLayer(relative, child->getHandle(), 1)
            .show(relative)
            .apply(true);

    // Captures mFGSurfaceControl layer and its children. Relative layer is a child of fg so its
    // relative value should be taken into account, placing it above child layer.
    ScreenCapture::captureLayers(&mCapture, fgHandle);
    mCapture->expectFGColor(10, 10);
    // Relative layer is showing on top of child layer
    mCapture->expectColor(Rect(0, 0, 9, 9), {100, 100, 100, 255});
}

TEST_F(ScreenCaptureTest, CaptureBoundlessLayerWithSourceCrop) {
    sp<SurfaceControl> child = createColorLayer("Child layer", Color::RED, mFGSurfaceControl.get());
    SurfaceComposerClient::Transaction().show(child).apply(true);

    sp<ISurfaceComposer> sf(ComposerService::getComposerService());
    Rect sourceCrop(0, 0, 10, 10);
    sp<IBinder> childHandle = child->getHandle();
    ScreenCapture::captureLayers(&mCapture, childHandle, sourceCrop);

    mCapture->expectColor(Rect(0, 0, 9, 9), Color::RED);
}

TEST_F(ScreenCaptureTest, CaptureBoundedLayerWithoutSourceCrop) {
    sp<SurfaceControl> child = createColorLayer("Child layer", Color::RED, mFGSurfaceControl.get());
    Rect layerCrop(0, 0, 10, 10);
    SurfaceComposerClient::Transaction().setCrop_legacy(child, layerCrop).show(child).apply(true);

    sp<ISurfaceComposer> sf(ComposerService::getComposerService());
    sp<GraphicBuffer> outBuffer;
    sp<IBinder> childHandle = child->getHandle();
    ScreenCapture::captureLayers(&mCapture, childHandle);

    mCapture->expectColor(Rect(0, 0, 9, 9), Color::RED);
}

TEST_F(ScreenCaptureTest, CaptureBoundlessLayerWithoutSourceCropFails) {
    sp<SurfaceControl> child = createColorLayer("Child layer", Color::RED, mFGSurfaceControl.get());
    SurfaceComposerClient::Transaction().show(child).apply(true);

    sp<ISurfaceComposer> sf(ComposerService::getComposerService());

    LayerCaptureArgs args;
    args.layerHandle = child->getHandle();

    ScreenCaptureResults captureResults;
    ASSERT_EQ(BAD_VALUE, sf->captureLayers(args, captureResults));
}

TEST_F(ScreenCaptureTest, CaptureBufferLayerWithoutBufferFails) {
    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    SurfaceComposerClient::Transaction().show(child).apply(true);

    sp<ISurfaceComposer> sf(ComposerService::getComposerService());
    sp<GraphicBuffer> outBuffer;

    LayerCaptureArgs args;
    args.layerHandle = child->getHandle();
    args.childrenOnly = false;

    ScreenCaptureResults captureResults;
    ASSERT_EQ(BAD_VALUE, sf->captureLayers(args, captureResults));

    TransactionUtils::fillSurfaceRGBA8(child, Color::RED);
    SurfaceComposerClient::Transaction().apply(true);
    ASSERT_EQ(NO_ERROR, sf->captureLayers(args, captureResults));
    ScreenCapture sc(captureResults.buffer);
    sc.expectColor(Rect(0, 0, 9, 9), Color::RED);
}

TEST_F(ScreenCaptureTest, CaptureLayerWithGrandchild) {
    auto fgHandle = mFGSurfaceControl->getHandle();

    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);

    sp<SurfaceControl> grandchild = createSurface(mClient, "Grandchild surface", 5, 5,
                                                  PIXEL_FORMAT_RGBA_8888, 0, child.get());

    TransactionUtils::fillSurfaceRGBA8(grandchild, 50, 50, 50);
    SurfaceComposerClient::Transaction()
            .show(child)
            .setPosition(grandchild, 5, 5)
            .show(grandchild)
            .apply(true);

    // Captures mFGSurfaceControl, its child, and the grandchild.
    ScreenCapture::captureLayers(&mCapture, fgHandle);
    mCapture->expectFGColor(10, 10);
    mCapture->expectChildColor(0, 0);
    mCapture->checkPixel(5, 5, 50, 50, 50);
}

TEST_F(ScreenCaptureTest, CaptureChildOnly) {
    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);
    auto childHandle = child->getHandle();

    SurfaceComposerClient::Transaction().setPosition(child, 5, 5).show(child).apply(true);

    // Captures only the child layer, and not the parent.
    ScreenCapture::captureLayers(&mCapture, childHandle);
    mCapture->expectChildColor(0, 0);
    mCapture->expectChildColor(9, 9);
}

TEST_F(ScreenCaptureTest, CaptureGrandchildOnly) {
    sp<SurfaceControl> child = createSurface(mClient, "Child surface", 10, 10,
                                             PIXEL_FORMAT_RGBA_8888, 0, mFGSurfaceControl.get());
    TransactionUtils::fillSurfaceRGBA8(child, 200, 200, 200);
    auto childHandle = child->getHandle();

    sp<SurfaceControl> grandchild = createSurface(mClient, "Grandchild surface", 5, 5,
                                                  PIXEL_FORMAT_RGBA_8888, 0, child.get());
    TransactionUtils::fillSurfaceRGBA8(grandchild, 50, 50, 50);

    SurfaceComposerClient::Transaction()
            .show(child)
            .setPosition(grandchild, 5, 5)
            .show(grandchild)
            .apply(true);

    auto grandchildHandle = grandchild->getHandle();

    // Captures only the grandchild.
    ScreenCapture::captureLayers(&mCapture, grandchildHandle);
    mCapture->checkPixel(0, 0, 50, 50, 50);
    mCapture->checkPixel(4, 4, 50, 50, 50);
}

TEST_F(ScreenCaptureTest, CaptureCrop) {
    sp<SurfaceControl> redLayer = createLayer(String8("Red surface"), 60, 60, 0);
    sp<SurfaceControl> blueLayer = createSurface(mClient, "Blue surface", 30, 30,
                                                 PIXEL_FORMAT_RGBA_8888, 0, redLayer.get());

    ASSERT_NO_FATAL_FAILURE(fillBufferQueueLayerColor(redLayer, Color::RED, 60, 60));
    ASSERT_NO_FATAL_FAILURE(fillBufferQueueLayerColor(blueLayer, Color::BLUE, 30, 30));

    SurfaceComposerClient::Transaction()
            .setLayer(redLayer, INT32_MAX - 1)
            .show(redLayer)
            .show(blueLayer)
            .apply(true);

    auto redLayerHandle = redLayer->getHandle();

    // Capturing full screen should have both red and blue are visible.
    ScreenCapture::captureLayers(&mCapture, redLayerHandle);
    mCapture->expectColor(Rect(0, 0, 29, 29), Color::BLUE);
    // red area below the blue area
    mCapture->expectColor(Rect(0, 30, 59, 59), Color::RED);
    // red area to the right of the blue area
    mCapture->expectColor(Rect(30, 0, 59, 59), Color::RED);

    const Rect crop = Rect(0, 0, 30, 30);
    ScreenCapture::captureLayers(&mCapture, redLayerHandle, crop);
    // Capturing the cropped screen, cropping out the shown red area, should leave only the blue
    // area visible.
    mCapture->expectColor(Rect(0, 0, 29, 29), Color::BLUE);
    mCapture->checkPixel(30, 30, 0, 0, 0);
}

TEST_F(ScreenCaptureTest, CaptureSize) {
    sp<SurfaceControl> redLayer = createLayer(String8("Red surface"), 60, 60, 0);
    sp<SurfaceControl> blueLayer = createSurface(mClient, "Blue surface", 30, 30,
                                                 PIXEL_FORMAT_RGBA_8888, 0, redLayer.get());

    ASSERT_NO_FATAL_FAILURE(fillBufferQueueLayerColor(redLayer, Color::RED, 60, 60));
    ASSERT_NO_FATAL_FAILURE(fillBufferQueueLayerColor(blueLayer, Color::BLUE, 30, 30));

    SurfaceComposerClient::Transaction()
            .setLayer(redLayer, INT32_MAX - 1)
            .show(redLayer)
            .show(blueLayer)
            .apply(true);

    auto redLayerHandle = redLayer->getHandle();

    // Capturing full screen should have both red and blue are visible.
    ScreenCapture::captureLayers(&mCapture, redLayerHandle);
    mCapture->expectColor(Rect(0, 0, 29, 29), Color::BLUE);
    // red area below the blue area
    mCapture->expectColor(Rect(0, 30, 59, 59), Color::RED);
    // red area to the right of the blue area
    mCapture->expectColor(Rect(30, 0, 59, 59), Color::RED);

    ScreenCapture::captureLayers(&mCapture, redLayerHandle, Rect::EMPTY_RECT, 0.5);
    // Capturing the downsized area (30x30) should leave both red and blue but in a smaller area.
    mCapture->expectColor(Rect(0, 0, 14, 14), Color::BLUE);
    // red area below the blue area
    mCapture->expectColor(Rect(0, 15, 29, 29), Color::RED);
    // red area to the right of the blue area
    mCapture->expectColor(Rect(15, 0, 29, 29), Color::RED);
    mCapture->checkPixel(30, 30, 0, 0, 0);
}

TEST_F(ScreenCaptureTest, CaptureInvalidLayer) {
    sp<SurfaceControl> redLayer = createLayer(String8("Red surface"), 60, 60, 0);

    ASSERT_NO_FATAL_FAILURE(fillBufferQueueLayerColor(redLayer, Color::RED, 60, 60));

    auto redLayerHandle = redLayer->getHandle();
    Transaction().reparent(redLayer, nullptr).apply();
    redLayer.clear();
    SurfaceComposerClient::Transaction().apply(true);

    LayerCaptureArgs args;
    args.layerHandle = redLayerHandle;

    ScreenCaptureResults captureResults;
    sp<ISurfaceComposer> sf(ComposerService::getComposerService());
    // Layer was deleted so captureLayers should fail with NAME_NOT_FOUND
    ASSERT_EQ(NAME_NOT_FOUND, sf->captureLayers(args, captureResults));
}

// In the following tests we verify successful skipping of a parent layer,
// so we use the same verification logic and only change how we mutate
// the parent layer to verify that various properties are ignored.
class ScreenCaptureChildOnlyTest : public ScreenCaptureTest {
public:
    void SetUp() override {
        ScreenCaptureTest::SetUp();

        mChild = createSurface(mClient, "Child surface", 10, 10, PIXEL_FORMAT_RGBA_8888, 0,
                               mFGSurfaceControl.get());
        TransactionUtils::fillSurfaceRGBA8(mChild, 200, 200, 200);

        SurfaceComposerClient::Transaction().show(mChild).apply(true);
    }

    void verify(std::function<void()> verifyStartingState) {
        // Verify starting state before a screenshot is taken.
        verifyStartingState();

        // Verify child layer does not inherit any of the properties of its
        // parent when its screenshot is captured.
        auto fgHandle = mFGSurfaceControl->getHandle();
        ScreenCapture::captureChildLayers(&mCapture, fgHandle);
        mCapture->checkPixel(10, 10, 0, 0, 0);
        mCapture->expectChildColor(0, 0);

        // Verify all assumptions are still true after the screenshot is taken.
        verifyStartingState();
    }

    std::unique_ptr<ScreenCapture> mCapture;
    sp<SurfaceControl> mChild;
};

// Regression test b/76099859
TEST_F(ScreenCaptureChildOnlyTest, CaptureLayerIgnoresParentVisibility) {
    SurfaceComposerClient::Transaction().hide(mFGSurfaceControl).apply(true);

    // Even though the parent is hidden we should still capture the child.

    // Before and after reparenting, verify child is properly hidden
    // when rendering full-screen.
    verify([&] { screenshot()->expectBGColor(64, 64); });
}

TEST_F(ScreenCaptureChildOnlyTest, CaptureLayerIgnoresParentCrop) {
    SurfaceComposerClient::Transaction()
            .setCrop_legacy(mFGSurfaceControl, Rect(0, 0, 1, 1))
            .apply(true);

    // Even though the parent is cropped out we should still capture the child.

    // Before and after reparenting, verify child is cropped by parent.
    verify([&] { screenshot()->expectBGColor(65, 65); });
}

// Regression test b/124372894
TEST_F(ScreenCaptureChildOnlyTest, CaptureLayerIgnoresTransform) {
    SurfaceComposerClient::Transaction().setMatrix(mFGSurfaceControl, 2, 0, 0, 2).apply(true);

    // We should not inherit the parent scaling.

    // Before and after reparenting, verify child is properly scaled.
    verify([&] { screenshot()->expectChildColor(80, 80); });
}

} // namespace android