# Anime Module API Reference

## Endpoint Used

- GET `/v2/users/{user_name}/animelist`

## Query Parameters Used

- `status=watching`
- `sort=anime_title`
- `limit=1000`
- `fields=list_status,num_episodes,broadcast,status,start_date`

## Authentication Modes

- `Authorization: Bearer <MAL_ACCESS_TOKEN>`
- `X-MAL-CLIENT-ID: <MAL_CLIENT_ID>`

## Required Response Fields

From each `data[]` item:

- `node.title`
- `node.num_episodes`
- `node.broadcast.day_of_the_week`
- `node.broadcast.start_time`
- `node.start_date`
- `list_status.num_episodes_watched` (or `list_status.num_watched_episodes`)

## Module Logic Summary

1. Read user entries with `watching` status.
2. Estimate released episode counts using `start_date` + weekly `broadcast` day/time.
3. Compare released counts at `now` and `now - 7 days`.
4. Mark entries where new episodes in this 7-day window exist and user has not watched them.
5. Return Waybar JSON:
   - class `has-updates` with text and tooltip list.
   - class `hidden` when no matches.
