// Exercise the production parser, OTA policy, updater UI and writer together.
#define main os_update_runtime_main
#include "test_update_runtime.c"
#undef main

static void load_original(const char *path) {
  FILE *f = fopen(path, "rb");
  assert(f);
  assert(!fseek(f, 0, SEEK_END));
  original_size = (size_t)ftell(f);
  rewind(f);
  original = malloc(original_size);
  assert(original && fread(original, 1, original_size, f) == original_size);
  fclose(f);
}

static void test_empty_candidate_list(void) {
  reset("update.zip");
  valid_package = packages[0].package;
  count = 0;
  int restarted = setjmp(mock_restart);
  if (!restarted)
    assert(!install());
  assert(!restarted && !mock_erases && !mock_allocations);
  puts("PASS: an empty candidate list stops before staging or flash erase");
}

typedef struct {
  journal saved_state;
  uint8_t bytes[32], staged[32];
  size_t size, staged_size;
  const esp_partition_t *boot, *previous_os;
  unsigned image_states[3];
} os_snapshot;

static void reset_confirmed_os(const char *name, unsigned running) {
  reset(name);
  mock_running = running;
  mock_boot = &mock_parts[running];
  mock_ota_states[0] = mock_ota_states[1] = ESP_OTA_IMG_VALID;
  state.active = running;
  state.previous = running ^ 1u;
  assert(save(&state) == ESP_OK);
  previous = freslest_ota_previous_os();
  assert(previous == &mock_parts[running ^ 1u]);
  snprintf(previous_version, sizeof(previous_version), "5.0.0");
  mock_nvs_sets = mock_nvs_commits = mock_boot_selections = 0;
  mock_begin_update_calls = mock_cancel_update_calls = mock_select_update_calls = 0;
  mock_events = 0;
}

static os_snapshot snapshot_os(void) {
  os_snapshot result = {.saved_state = state,
                        .size = mock_saved_size,
                        .staged_size = mock_staged_size,
                        .boot = mock_boot,
                        .previous_os = previous};
  memcpy(result.bytes, mock_saved, sizeof(result.bytes));
  memcpy(result.staged, mock_staged, sizeof(result.staged));
  memcpy(result.image_states, mock_ota_states, sizeof(result.image_states));
  return result;
}

static void assert_os_unchanged(const os_snapshot *before) {
  assert(!mock_nvs_sets && !mock_nvs_commits && !mock_boot_selections);
  assert(!mock_begin_update_calls && !mock_cancel_update_calls && !mock_select_update_calls);
  assert(mock_boot == before->boot && previous == before->previous_os);
  assert(!memcmp(&state, &before->saved_state, sizeof(state)));
  assert(mock_saved_size == before->size && mock_staged_size == before->staged_size);
  assert(!memcmp(mock_saved, before->bytes, sizeof(before->bytes)));
  assert(!memcmp(mock_staged, before->staged, sizeof(before->staged)));
  assert(!memcmp(mock_ota_states, before->image_states, sizeof(before->image_states)));
  assert(!mock_destination || mock_destination == &mock_doom);
}

static void expect_restart_on_key(unsigned key) {
  int reason = setjmp(mock_restart);
  if (!reason) {
    press(key);
    assert(!"The updater must restart");
  }
  assert(reason == 1);
}

static void test_target_visible_in_prompts(void) {
  uint8_t region[480 * 32];
  reset_confirmed_os("doom.zip", 0);
  draw();
  memcpy(region, screen + 78 * 480, 480 * 14);
  packages[0].package.target = FRESLEST_UPDATE_OS;
  draw();
  assert(memcmp(region, screen + 78 * 480, 480 * 14));
  packages[0].package.target = FRESLEST_UPDATE_DOOM;
  confirm = true;
  draw();
  memcpy(region, screen + 142 * 480, sizeof(region));
  packages[0].package.target = FRESLEST_UPDATE_OS;
  draw();
  assert(memcmp(region, screen + 142 * 480, sizeof(region)));
  active_target = FRESLEST_UPDATE_DOOM;
  last_percent = -1;
  progress(56, "Writing the single Doom slot");
  capture("native-doom-progress.raw");
  memcpy(region, screen + 76 * 480, 480 * 14);
  active_target = FRESLEST_UPDATE_OS;
  last_percent = -1;
  progress(56, "Writing the single Doom slot");
  assert(memcmp(region, screen + 76 * 480, 480 * 14));
  puts("PASS: update list, final warning and progress heading distinguish OS and Doom targets");
}

static void test_doom_cancel(void) {
  for (unsigned running = 0; running < 2; ++running) {
    for (unsigned confirmed = 0; confirmed < 2; ++confirmed) {
      for (unsigned disconnected = 0; disconnected < 2; ++disconnected) {
        reset_confirmed_os("doom.zip", running);
        // A different boot destination must also remain untouched by Doom BACK.
        mock_boot = &mock_parts[running ^ 1u];
        os_snapshot before = snapshot_os();
        if (confirmed)
          select_package();
        if (disconnected)
          unmount_drive();
        expect_restart_on_key(3);
        assert(!mock_erases && !mock_writes && !mock_allocations);
        assert_os_unchanged(&before);
      }
    }
  }
  puts("PASS: Doom selection/confirmation cancellation from OS A/B preserves boot, NVS and previous OS");
}

static void test_doom_success_and_cuts(const char *doom_path) {
  free(original);
  load_original(doom_path);
  for (unsigned running = 0; running < 2; ++running) {
    reset_confirmed_os("doom.zip", running);
    os_snapshot before = snapshot_os();
    draw();
    if (!running)
      capture("native-doom-list.raw");
    select_package();
    if (!running)
      capture("native-doom-confirm.raw");
    expect_restart_on_key(2);
    assert(last_percent == 100 && mock_destination == &mock_parts[2]);
    assert(mock_erases && mock_writes && !mock_allocations);
    assert(mock_first_erase_mode == IDLE && !memcmp(mock_flash, original, original_size));
    assert_os_unchanged(&before);
    unsigned events = mock_events;
    for (volatile unsigned cut = 1; cut <= events; ++cut) {
      reset_confirmed_os("doom.zip", running);
      before = snapshot_os();
      select_package();
      mock_cut_at = cut;
      int reason = setjmp(mock_restart);
      if (!reason) {
        press(2);
        assert(!"A simulated power cut must interrupt the installation");
      }
      assert(reason == 2);
      mock_cut_at = 0;
      assert_os_unchanged(&before);
    }
    printf("PASS: OS %c Doom update and all %u erase/write/end/read-back cutpoints preserve boot, NVS and previous OS\n",
           'A' + running, events);
  }
}

static void test_doom_failures(void) {
  for (unsigned running = 0; running < 2; ++running) {
    for (unsigned failure = 0; failure < 6; ++failure) {
      reset_confirmed_os("doom.zip", running);
      os_snapshot before = snapshot_os();
      if (failure == 0) mock_heap_fail = true;
      if (failure == 1) mock_begin_fail = true;
      if (failure == 2) mock_write_failure = 2;
      if (failure == 3) mock_end_fail = true;
      if (failure == 4) mock_read_corrupt = true;
      if (failure == 5) mock_read_fail = true;
      select_package();
      press(2);
      assert(failed && last_percent < 100 && !mock_allocations);
      assert_os_unchanged(&before);
      expect_restart_on_key(3);
      assert_os_unchanged(&before);
    }
  }
  puts("PASS: Doom heap/OTA begin/write/end/read corruption/read failure and BACK preserve OS state");
}

static void test_doom_validation(void) {
  const char *invalid[] = {"bad-doom-sha.zip", "wrong-project.zip", "corrupt-doom.zip"};
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
    reset_confirmed_os(invalid[i], 0);
    os_snapshot before = snapshot_os();
    select_package();
    press(2);
    assert(failed && !mock_erases && !mock_writes && !mock_allocations);
    assert_os_unchanged(&before);
  }
  const char *changed[] = {"changed-target.zip", "changed-version.zip", "changed-size.zip", "changed-sha.zip"};
  for (unsigned i = 0; i < sizeof(changed) / sizeof(changed[0]); ++i) {
    reset_confirmed_os("doom.zip", 0);
    os_snapshot before = snapshot_os();
    snprintf(packages[0].filename, sizeof(packages[0].filename), "%s", changed[i]);
    select_package();
    press(2);
    assert(failed && !mock_erases && !mock_writes && !mock_allocations);
    assert(strstr(error_text, "ZIP changed"));
    assert_os_unchanged(&before);
  }
  puts("PASS: corrupt/wrong-project and changed target/version/size/SHA stop before the first erase");
}

static void test_doom_writer_preconditions(void) {
  for (unsigned invalid = 0; invalid < 8; ++invalid) {
    reset_confirmed_os("doom.zip", 0);
    esp_partition_t saved_doom = mock_doom;
    if (invalid == 0) mock_doom.address++;
    if (invalid == 1) mock_doom.size++;
    if (invalid == 2) mock_doom.type = ESP_PARTITION_TYPE_DATA;
    if (invalid == 3) mock_doom.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
    if (invalid == 4) mock_doom.label = "other";
    if (invalid == 5) mock_bad_running = true;
    if (invalid == 6) {
      pending = true;
      ready = false;
      mock_ota_states[0] = ESP_OTA_IMG_PENDING_VERIFY;
    }
    if (invalid == 7) packages[0].package.target = (enum freslest_update_target)99;
    os_snapshot before = snapshot_os();
    select_package();
    press(2);
    assert(failed && !mock_erases && !mock_writes && !mock_allocations);
    assert_os_unchanged(&before);
    mock_doom = saved_doom;
  }
  reset_confirmed_os("doom.zip", 0);
  selected = MAX_PACKAGES;
  assert(!install() && !mock_erases && !mock_allocations);
  selected = UINT32_MAX;
  assert(!install() && !mock_erases && !mock_allocations);
  count = MAX_PACKAGES + 1;
  selected = 0;
  assert(!install() && !mock_erases && !mock_allocations);
  puts("PASS: candidate bounds, exact Doom layout, running Doom and pending OS reject before staging/erase");
}

static void test_os_b_regression(const char *os_path) {
  free(original);
  load_original(os_path);
  reset_confirmed_os("update.zip", 1);
  valid_package = packages[0].package;
  select_package();
  expect_restart_on_key(3);
  assert(!mock_erases && !mock_writes && mock_boot == &mock_parts[1] && state.mode == IDLE);
  reset_confirmed_os("update.zip", 1);
  valid_package = packages[0].package;
  select_package();
  expect_restart_on_key(2);
  assert(mock_destination == &mock_parts[0] && mock_boot == &mock_parts[0]);
  assert(mock_begin_update_calls == 1 && mock_select_update_calls == 1);
  assert(state.mode == TRIAL && state.source == 1 && state.target == 0 && state.previous == 255);
  assert(!mock_allocations && !memcmp(mock_flash, original, original_size));
  puts("PASS: OS B cancellation and inactive OS A trial update retain the legacy A/B behavior");
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(argc == 3);
  load_original(argv[1]);
  mock_flash = malloc(FRESLEST_OS_SLOT_SIZE);
  screen = calloc(480 * 320, 1);
  assert(mock_flash && screen);
  test_empty_candidate_list();
  test_target_visible_in_prompts();
  test_doom_cancel();
  test_doom_success_and_cuts(argv[2]);
  test_doom_failures();
  test_doom_validation();
  test_doom_writer_preconditions();
  test_os_b_regression(argv[1]);
  free(original);
  free(mock_flash);
  free(screen);
  return 0;
}
