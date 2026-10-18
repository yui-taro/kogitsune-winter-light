# ブラウザ版で Jev を使う（中継を Render に置く）

ブラウザ版（https://yui-taro.github.io/kogitsune-winter-light/ ）の狛犬を Jev で動かすための手順です。API キーとパスワードは Render の管理画面にだけ入れます。ゲームやこのリポジトリには入れません。

## 仕組み

```
ブラウザ（公開ページ）
  │ ① パスワードでログイン → 期限付きの通行証（12時間）を受け取る
  │ ② 狛犬の判断が必要なときだけ、通行証つきで観測を送る
  ▼
中継（Render 上の server/relay.mjs）  ← API キー・パスワードはここだけ
  │ 通行証の確認、回数制限、ブラウザの接続元の確認
  ▼
Jev API（料金はあなたのアカウントに請求）
```

ログインしない人は、今までどおり狛犬がローカル判断で動きます。中継が止まっても上限に達しても、ゲームは自動でローカル判断に切り替わります。

## 1. Render に中継を作る（最初の1回）

1. https://render.com でアカウントを作り、GitHub と連携します。
2. ダッシュボードで **New → Blueprint** を選び、リポジトリ `yui-taro/kogitsune-winter-light` を選びます。
3. リポジトリの `render.yaml` が読み込まれ、`kogitsune-jev-relay` が表示されます。次の2つを入力します。
   - `TYPESAFE_API_KEY`：TypeSafe の API キー
   - `RELAY_PASSWORDS`：ログイン用のパスワード（8文字以上）。複数作る場合はカンマで区切ります（例：`yuki-akari-2026,tomodachi-pass-01`）。人ごとに分けておくと、あとで1人分だけ無効にできます。
4. **Apply** を押すと中継が立ち上がります。表示される URL（例：`https://kogitsune-jev-relay.onrender.com`）を控えます。
5. ブラウザで `https://（控えたURL）/health` を開き、`{"ready":true,"mode":"jev","login":true}` と表示されれば準備完了です。

## 2. 公開ページに中継の URL を設定する

GitHub のリポジトリで **Settings → Secrets and variables → Actions → Variables** を開き、**New repository variable** で次を追加します。

- Name：`JEV_RELAY_URL`
- Value：手順1-4で控えた URL（末尾の `/` は不要）

追加したら **Actions → Web build (GitHub Pages) → Run workflow** で公開ページを作り直します。読み込み画面にログイン欄が出るようになります。

## 3. 費用を抑える設定

Render の管理画面の **Environment** で変更できます（変更すると中継が再起動します）。

| 変数 | 初期値 | 意味 |
| --- | --- | --- |
| `RELAY_DAILY_LIMIT` | 1000 | 1日（UTC）あたりの Jev 判断の上限。超えると翌日まで全員ローカル判断になります |
| `RELAY_PER_MINUTE_LIMIT` | 20 | ログイン1回あたり、1分間の判断の上限 |
| `RELAY_SESSION_HOURS` | 12 | ログインが有効な時間 |
| `RELAY_PASSWORDS` | （入力） | パスワード。消すと、そのパスワードでは新しくログインできなくなります |
| `RELAY_SESSION_SECRET` | 自動生成 | 通行証の署名鍵。作り直すと、全員のログインがその場で無効になります |

パスワードを10回続けて間違えた接続元は、15分間ログインできなくなります。

使用量は Render の **Logs** で確認できます。Jev を1回呼ぶごとに `{"event":"jev_usage", ... "today":回数}` が1行出ます。上限で断ったときは `jev_refused` が出ます。パスワードや API キーはログに出ません。

## 注意

- **無料プランの休止：** 無料プランの中継は、15分ほど使われないと休止します。次のログインで起こすのに最大1分ほどかかります（ページでは「サーバーを起こしています」と表示）。待たせたくない場合は有料プランにします。
- **料金：** 料金は Jev の呼び出し回数で決まります。狛犬は状況が変わったときだけ判断を求めるので、1回のプレイでの呼び出しは多くありません。最初は上限を低めにして、Logs の回数を見ながら調整してください。
- **ダウンロード版：** `RELAY_PASSWORDS` を設定した中継はログインが必要です。ダウンロード版（Mac／Windows）からは使えないので、ダウンロード版では今までどおり手元で中継を起動してください（`game/README.md` 参照）。
