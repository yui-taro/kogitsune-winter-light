# 4. チェックポイントとテスト

捕獲された後のやり直しは、子狐だけを元へ戻しても成立しません。狛犬の記憶、姿、足跡、灯籠も同じ時点に戻す必要があります。これは [D-18](../design/decision-log.md) の判断です。

## 二つの保存地点

- **CP-B**: 神社へ入った時の `WorldState`。まだ灯籠を取る前に捕獲された場合に使います。
- **CP-C**: 境内の手前で最初に保存する `WorldState`。灯籠を取った後に捕獲された場合に使います。灯籠を取る瞬間まで C がなければ、その直前にも保存します。

`WorldState` のコピーに `Knowledge`、足跡、狛犬、灯籠も含まれます。`RestoreCheckpoint` は保存状態を戻し、古い `pending_` / `inFlight_` を消し、`attemptId` を増やします。

## 古い返信を拒否する三つの値

| 値 | 変わる時 | 防ぐ問題 |
| --- | --- | --- |
| `attemptId` | 再挑戦・最初から再開 | 前の試行への返信が新しい試行に混ざる |
| `decisionId` | 判断要求を作るたび | 別の要求への返信を取り違える |
| `revision` | 狛犬の知識に意味のある変化があった時 | 新しい観測より古い判断を適用する |

さらに `ApplyDecision` は、返ってきた意図が要求時の `legalIntents` に入るか調べます。たとえば鈴を聞いただけの狛犬が「見えている狐を追う」と返しても採用しません。

## テストを読む順番

| テスト | 確かめること |
| --- | --- |
| `tests/test_contract.cpp` の `DecisionIdentityAndLegality` | 3種類の値・候補外・二重適用の拒否 |
| 同 `CheckpointRestoresWholeSnapshot` | 灯籠・記憶・狛犬・足跡の復元 |
| 同 `CheckpointBAndCapturedFreeze` | CP-B へ戻ること、捕獲中は動かないこと |
| `tests/test_world.cpp` の `FullStoryReachesEnding` | 巣から結末まで場面がつながること |
| `server/test.mjs` と `tests/test_bridge.cpp` | 模擬中継と C++ クライアントの HTTP 接続 |

```sh
make -C game test
make -C game build/test_bridge
cd server && node --test test.mjs
```

最後のテストは `127.0.0.1` のポートを使います。実行環境がローカル通信を制限する場合は許可が必要です。TypeSafe の本番 API を使うテストではありません。

## 自分で追う実験

`CheckpointRestoresWholeSnapshot` で、`saved` を作った直後と `R` 相当の `restart` を送った後を比べます。`fox.position` 以外に何が同じで、`attemptId` だけなぜ変わるかを説明してください。
