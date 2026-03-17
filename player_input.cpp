#include "player.h"
#include "game_manager.h"
#include "define.h"
#include <DxLib.h>

// Player::ProcessInput の実装ファイル
// このファイルにはプレイヤーの入力取得と「瞬間押下(JustPressed)」判定を切り出しています。
// 詳細コメント（日本語）を多めに入れて、誰が見ても挙動が分かるようにしています。

/**
 * 入力処理の目的:
 * - キーボード、パッド、マウスからの入力を収集する
 * - "押している" と "瞬間押下(JustPressed)" を判定する
 * - EditMode のときはマウス入力を無視する
 * - Player クラスの状態フラグ (wasQPressed, jumpKeyPressed, attackKeyPressed, dodgeKeyPressed)
 *   を更新して、Update() 側で利用できるようにする
 *
 * 実装上の注意:
 * - 元の Update() 内で利用していた keyStateArr をメンバーに移動しているため、
 *   ここで GetHitKeyStateAll() を呼んでメンバーへ格納する。
 * - パッド入力は GetJoypadInputState() で取得する。
 */
Player::InputState Player::ProcessInput() {
    InputState out;

    // パッド入力を先に取得
    int padInput = GetJoypadInputState(DX_INPUT_PAD1);
    out.padInput = padInput;

    // キーの押下状態を配列に格納（GetHitKeyStateAll は 256 バイトの配列を要求する）
    GetHitKeyStateAll(keyStateArr);

    // キーボード上の特定キーの押下をチェック
    bool qDown = (keyStateArr[KEY_INPUT_Q] != 0);
    bool spaceDown = (keyStateArr[KEY_INPUT_SPACE] != 0);

    // パッドの対応ボタンをチェック
    bool padYDown = ((padInput & PAD_INPUT_Y) != 0);
    bool padADown = ((padInput & PAD_INPUT_A) != 0);

    // Q: 撃つボタン (キーボードQ / パッドY)
    // wasQPressed は Player::Update() のフローで更新されるため、瞬間押下は "現在押されている AND 前回は押されていなかった"
    out.isQJustPressed = (qDown || padYDown) && !wasQPressed;

    // ジャンプボタン (Space / パッド A)
    out.isWJustPressed = (spaceDown || padADown) && !jumpKeyPressed;
    // 現在押されているかどうかはメンバーへ反映しておく
    jumpKeyPressed = (spaceDown || padADown);

    // マウス状態を取得し、EditMode の場合はマウス入力を無効化する
    int mouseState = GetMouseInput();
    if (GameManager::GetInstance().editModeManager.IsEditMode()) {
        mouseState = 0; // エディットモード時はマウス無効
    }

    // Swap mouse buttons: left click = attack (X), right click = dodge (Z)
    // 攻撃: 左クリックまたはパッドの X ボタン
    out.isXPressed = ((mouseState & MOUSE_INPUT_LEFT) != 0) || (padInput & PAD_INPUT_X);
    out.isXJustPressed = out.isXPressed && !attackKeyPressed;
    attackKeyPressed = out.isXPressed;

    // 回避: 右クリックまたはパッドの B ボタン
    out.isZPressed = ((mouseState & MOUSE_INPUT_RIGHT) != 0) || (padInput & PAD_INPUT_B);
    out.isZJustPressed = out.isZPressed && !dodgeKeyPressed;
    dodgeKeyPressed = out.isZPressed;

    return out;
}
